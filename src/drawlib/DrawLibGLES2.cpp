/*
 * DrawLibGLES2.cpp — X-Moto GLES2 / WebGL2 rendering backend (Phase 5).
 *
 * Design:
 *   - One GLSL ES 1.00 shader pair for all 2D draws.
 *   - CPU-side vertex batch flushed on endDraw() / state change.
 *   - Manual 4x4 float matrix stack (projection + model).
 *   - Font rendering via SDL_ttf + per-glyph GL textures.
 */

#ifdef __EMSCRIPTEN__

#include "DrawLibGLES2.h"
#include "common/Image.h"
#include "common/VFileIO.h"
#include "common/VTexture.h"
#include "common/XMBuild.h"
#include "helpers/Log.h"
#include "helpers/RenderSurface.h"
#include "helpers/utf8.h"
#include "include/xm_hashmap.h"
#include "xmscene/Camera.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GLES2/gl2.h>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

/* =========================================================================
   Shader sources (GLSL ES 1.00 / WebGL 1.0 compatible)
   ========================================================================= */

static const char *VERT_SRC = R"(
attribute vec2  a_pos;
attribute vec2  a_uv;
attribute vec4  a_color;

uniform   mat4  u_mvp;

varying   vec2  v_uv;
varying   vec4  v_color;

void main() {
    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
    v_uv        = a_uv;
    v_color     = a_color;
}
)";

static const char *FRAG_SRC = R"(
precision mediump float;

uniform sampler2D u_tex;
uniform int       u_use_tex;

varying vec2 v_uv;
varying vec4 v_color;

void main() {
    if (u_use_tex == 1) {
        gl_FragColor = texture2D(u_tex, v_uv) * v_color;
    } else {
        gl_FragColor = v_color;
    }
}
)";

/* =========================================================================
   Vertex layout — defined in DrawLibGLES2.h, referenced here
   ========================================================================= */

/* =========================================================================
   Minimal 4x4 float matrix helpers  (column-major, like OpenGL)
   ========================================================================= */

static void mat4_identity(float *m) {
  memset(m, 0, 16 * sizeof(float));
  m[0] = m[5] = m[10] = m[15] = 1.f;
}

static void mat4_multiply(float *out, const float *a, const float *b) {
  float tmp[16];
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      float s = 0.f;
      for (int k = 0; k < 4; ++k)
        s += a[row + k * 4] * b[k + col * 4];
      tmp[row + col * 4] = s;
    }
  }
  memcpy(out, tmp, 16 * sizeof(float));
}

static void mat4_ortho(float *m,
                       float l, float r, float b, float t,
                       float n, float f) {
  mat4_identity(m);
  m[0]  =  2.f / (r - l);
  m[5]  =  2.f / (t - b);
  m[10] = -2.f / (f - n);
  m[12] = -(r + l) / (r - l);
  m[13] = -(t + b) / (t - b);
  m[14] = -(f + n) / (f - n);
}

static void mat4_translate(float *m, float tx, float ty) {
  float t[16]; mat4_identity(t);
  t[12] = tx; t[13] = ty;
  float tmp[16]; memcpy(tmp, m, 64);
  mat4_multiply(m, tmp, t);
}

static void mat4_scale(float *m, float sx, float sy) {
  float s[16]; mat4_identity(s);
  s[0] = sx; s[5] = sy;
  float tmp[16]; memcpy(tmp, m, 64);
  mat4_multiply(m, tmp, s);
}

static void mat4_rotate_z(float *m, float angle_deg) {
  float rad = angle_deg * (float)M_PI / 180.f;
  float c = cosf(rad), s = sinf(rad);
  float r[16]; mat4_identity(r);
  r[0] = c; r[4] = -s;
  r[1] = s; r[5] =  c;
  float tmp[16]; memcpy(tmp, m, 64);
  mat4_multiply(m, tmp, r);
}

/* =========================================================================
   Shader compilation helpers
   ========================================================================= */

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char buf[512]; glGetShaderInfoLog(s, 512, nullptr, buf);
    LogError("DrawLibGLES2 shader compile error: %s", buf);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

static GLuint link_program(GLuint vert, GLuint frag) {
  GLuint p = glCreateProgram();
  glAttachShader(p, vert);
  glAttachShader(p, frag);
  glLinkProgram(p);
  GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
    LogError("DrawLibGLES2 link error: %s", buf);
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

/* =========================================================================
   GLES2 Font classes  (replace the desktop GLFontGlyph / GLFontManager)
   ========================================================================= */

#define UTF8_INTERLINE_SPACE 2
#define UTF8_INTERCHAR_SPACE 0

static int next_power_of2(int v) {
  int p = 1; while (p < v) p <<= 1; return p;
}

static SDL_Surface *make_rgba_surface(int w, int h) {
  return SDL_CreateRGBSurface(0, w, h, 32,
    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
}

class GLES2FontGlyphLetter : public FontGlyph {
public:
  GLuint   m_texID;
  Vector2f m_u, m_v;   /* UV corners of the glyph inside the texture */
  unsigned int m_drawW, m_drawH, m_realW, m_realH;

  GLES2FontGlyphLetter(const std::string &i_value, TTF_Font *ttf, int fixedW)
      : m_texID(0) {
    SDL_Color white = { 255, 255, 255, 0 };
    SDL_Surface *surf = TTF_RenderUTF8_Blended(ttf, i_value.c_str(), white);
    if (!surf) {
      surf = TTF_RenderUTF8_Blended(ttf, " ", white);
      if (!surf) throw Exception("GLES2FontGlyphLetter TTF failed");
    }
    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);

    int rw, rh;
    TTF_SizeUTF8(ttf, i_value.c_str(), &rw, &rh);
    m_realW = (unsigned int)rw;
    m_realH = (unsigned int)rh;
    m_drawW = m_realW;
    m_drawH = m_realH;
    if (fixedW > 0 && TTF_FontFaceIsFixedWidth(ttf))
      m_drawW = m_realW = (unsigned int)fixedW;

    /* Upload to a power-of-two texture */
    int tw = next_power_of2((int)m_drawW);
    int th = next_power_of2((int)m_drawH);
    SDL_Surface *img = make_rgba_surface(tw, th);
    SDL_BlitSurface(surf, nullptr, img, nullptr);
    SDL_FreeSurface(surf);

    glGenTextures(1, &m_texID);
    glBindTexture(GL_TEXTURE_2D, m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
    SDL_FreeSurface(img);

    m_u = { 0.f, 0.f };
    m_v = { (float)m_drawW / tw, (float)m_drawH / th };
  }

  ~GLES2FontGlyphLetter() { if (m_texID) glDeleteTextures(1, &m_texID); }

  unsigned int realWidth()  const override { return m_realW; }
  unsigned int realHeight() const override { return m_realH; }
};

class GLES2FontGlyph : public FontGlyph {
public:
  std::string m_value;
  unsigned int m_drawW, m_drawH, m_realW, m_realH, m_firstLineH;

  GLES2FontGlyph(const std::string &v,
    HashNamespace::unordered_map<std::string, GLES2FontGlyphLetter *> &letters)
      : m_value(v), m_drawW(0), m_drawH(0), m_realW(0), m_realH(0), m_firstLineH(0) {
    if (v.empty()) return;

    bool firstLine = true;
    unsigned int maxH = 0, curW = 0;
    unsigned int n = 0;
    std::string ch;
    while (n < v.size()) {
      ch = utf8::getNextChar(v, n);
      if (ch == "\n") {
        if (firstLine) { m_firstLineH = maxH; firstLine = false; }
        m_realH += UTF8_INTERLINE_SPACE + maxH;
        maxH = 0; curW = 0;
      } else {
        auto *l = letters[ch];
        if (l) {
          if (l->realHeight() > maxH) maxH = l->realHeight();
          if (curW) curW += UTF8_INTERCHAR_SPACE;
          curW += l->realWidth();
          if (curW > m_realW) m_realW = curW;
        }
      }
    }
    if (ch != "\n") m_realH += maxH;
    if (firstLine) m_firstLineH = maxH;
    m_drawW = m_realW; m_drawH = m_realH;
  }

  unsigned int realWidth()  const override { return m_realW; }
  unsigned int realHeight() const override { return m_realH; }
  unsigned int drawWidth()  const { return m_drawW; }
  unsigned int drawHeight() const { return m_drawH; }
  unsigned int firstLineH() const { return m_firstLineH; }
};

class GLES2FontManager : public FontManager {
public:
  GLES2FontManager(DrawLib *dl, const std::string &file, unsigned int size,
                   unsigned int fixedW = 0)
      : FontManager(dl, file, size, fixedW) {}

  ~GLES2FontManager() {
    for (auto *g  : m_glyphVals)  delete g;
    for (auto *gl : m_letterVals) delete gl;
  }

  FontGlyph *getGlyph(const std::string &str) override;
  FontGlyph *getGlyphTabExtended(const std::string &str) override;

  void printString(DrawLib *dl, FontGlyph *g, int x, int y,
                   Color c, float perCentered, bool shadow) override;
  void printStringGrad(DrawLib *dl, FontGlyph *g, int x, int y,
                       Color c1, Color c2, Color c3, Color c4,
                       float perCentered, bool shadow) override;

  void displayScrap(DrawLib *) override {}
  unsigned int nbGlyphsInMemory() { return (unsigned)m_letterVals.size(); }

private:
  void ensureLetter(const std::string &ch);
  void printStringGradOne(DrawLib *dl, FontGlyph *g, int x, int y,
                          Color c1, Color c2, Color c3, Color c4,
                          float perCentered);

  unsigned int getLongestLineSize(const std::string &v, unsigned int start = 0,
                                   unsigned int lines = 0);

  /* glyph cache */
  std::vector<std::string>            m_glyphKeys;
  std::vector<GLES2FontGlyph *>       m_glyphVals;
  HashNamespace::unordered_map<std::string, GLES2FontGlyph *> m_glyphs;

  std::vector<std::string>            m_letterKeys;
  std::vector<GLES2FontGlyphLetter *> m_letterVals;
  HashNamespace::unordered_map<std::string, GLES2FontGlyphLetter *> m_letters;
};

void GLES2FontManager::ensureLetter(const std::string &ch) {
  if (m_letters[ch]) return;
  auto *l = new GLES2FontGlyphLetter(ch, m_ttf, (int)m_fixedFontSize);
  m_letterKeys.push_back(ch);
  m_letterVals.push_back(l);
  m_letters[m_letterKeys.back()] = l;
}

FontGlyph *GLES2FontManager::getGlyphTabExtended(const std::string &str) {
  std::string ext;
  unsigned int n = 0, tab = 5;
  std::string ch;
  while (n < str.size()) {
    ch = utf8::getNextChar(str, n);
    if (ch == "\t") { for (unsigned i = n % tab; i < tab; ++i) ext += ' '; }
    else ext += ch;
  }
  return getGlyph(ext);
}

FontGlyph *GLES2FontManager::getGlyph(const std::string &str) {
  if (m_glyphs[str]) return m_glyphs[str];
  unsigned int n = 0;
  std::string ch;
  while (n < str.size()) {
    ch = utf8::getNextChar(str, n);
    if (ch != "\n") ensureLetter(ch);
  }
  auto *g = new GLES2FontGlyph(str, m_letters);
  m_glyphKeys.push_back(str);
  m_glyphVals.push_back(g);
  m_glyphs[m_glyphKeys.back()] = g;
  return g;
}

unsigned int GLES2FontManager::getLongestLineSize(const std::string &v,
    unsigned int start, unsigned int lines) {
  unsigned int longest = 0, cur = 0, lineN = 0, n = start;
  std::string ch;
  while (n < v.size()) {
    ch = utf8::getNextChar(v, n);
    if (ch == "\n") {
      if (cur > longest) longest = cur;
      cur = 0; lineN++;
      if (lines && lineN == lines) return longest;
    } else {
      auto *l = m_letters[ch];
      if (l) cur += l->realWidth() + UTF8_INTERCHAR_SPACE;
    }
  }
  return cur > longest ? cur : longest;
}

void GLES2FontManager::printString(DrawLib *dl, FontGlyph *fg, int x, int y,
                                    Color c, float perCentered, bool shadow) {
  if (shadow) {
    printStringGradOne(dl, fg, x, y,
                       INVERT_COLOR(c), INVERT_COLOR(c), INVERT_COLOR(c), INVERT_COLOR(c),
                       perCentered);
    printStringGradOne(dl, fg, x + 1, y + 1, c, c, c, c, perCentered);
  } else {
    printStringGradOne(dl, fg, x, y, c, c, c, c, perCentered);
  }
}

void GLES2FontManager::printStringGrad(DrawLib *dl, FontGlyph *fg, int x, int y,
    Color c1, Color c2, Color c3, Color c4, float perCentered, bool shadow) {
  if (shadow) {
    printStringGradOne(dl, fg, x, y,
                       INVERT_COLOR(c1), INVERT_COLOR(c2), INVERT_COLOR(c3), INVERT_COLOR(c4),
                       perCentered);
    printStringGradOne(dl, fg, x + 1, y + 1, c1, c2, c3, c4, perCentered);
  } else {
    printStringGradOne(dl, fg, x, y, c1, c2, c3, c4, perCentered);
  }
}

/* =========================================================================
   DrawLibGLES2 — implementation
   ========================================================================= */

DrawLibGLES2::DrawLibGLES2()
    : DrawLib(), m_glContext(nullptr), m_prog(0), m_vbo(0),
      m_loc_mvp(-1), m_loc_use_tex(-1), m_loc_tex(-1),
      m_loc_pos(-1), m_loc_uv(-1), m_loc_color(-1),
      m_cur_r(255), m_cur_g(255), m_cur_b(255), m_cur_a(255),
      m_cur_u(0), m_cur_v(0), m_drawMode(DRAW_MODE_NONE) {
  mat4_identity(m_proj);
  mat4_identity(m_model);
  mat4_identity(m_mvp);
}

DrawLibGLES2::~DrawLibGLES2() {}

void DrawLibGLES2::init(unsigned int nW, unsigned int nH, bool windowed) {
  DrawLib::init(nW, nH, windowed);
  m_nDispWidth  = nW;
  m_nDispHeight = nH;
  m_bWindowed   = true; /* always windowed on web */

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  std::string title = "X-Moto " + XMBuild::getVersionString(true);
  m_window = SDL_CreateWindow(title.c_str(),
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              (int)nW, (int)nH,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
  if (!m_window)
    throw Exception("DrawLibGLES2: SDL_CreateWindow: " + std::string(SDL_GetError()));

  m_glContext = SDL_GL_CreateContext(m_window);
  if (!m_glContext)
    throw Exception("DrawLibGLES2: SDL_GL_CreateContext: " + std::string(SDL_GetError()));

  /* compile shaders */
  GLuint vert = compile_shader(GL_VERTEX_SHADER,   VERT_SRC);
  GLuint frag = compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC);
  if (!vert || !frag)
    throw Exception("DrawLibGLES2: shader compilation failed");
  m_prog = link_program(vert, frag);
  glDeleteShader(vert);
  glDeleteShader(frag);
  if (!m_prog)
    throw Exception("DrawLibGLES2: shader link failed");

  /* cache uniform / attribute locations */
  m_loc_mvp     = glGetUniformLocation(m_prog, "u_mvp");
  m_loc_use_tex = glGetUniformLocation(m_prog, "u_use_tex");
  m_loc_tex     = glGetUniformLocation(m_prog, "u_tex");
  m_loc_pos     = glGetAttribLocation(m_prog,  "a_pos");
  m_loc_uv      = glGetAttribLocation(m_prog,  "a_uv");
  m_loc_color   = glGetAttribLocation(m_prog,  "a_color");

  /* streaming VBO for batch uploads */
  glGenBuffers(1, &m_vbo);

  /* set up 2D ortho: screen coords (0,0) at top-left */
  mat4_ortho(m_proj, 0.f, (float)nW, 0.f, (float)nH, -1.f, 1.f);
  mat4_identity(m_model);
  _recompute_mvp();

  m_menuCamera = new Camera(Vector2i(0, 0), Vector2i((int)nW, (int)nH));
  m_menuCamera->setCamera2d();

  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  SDL_GL_SwapWindow(m_window);

  /* pre-load fonts */
  m_fontSmall    = getFontManager(XMFS::FullPath(FDT_DATA, FontManager::getDrawFontFile()), 14);
  m_fontMedium   = getFontManager(XMFS::FullPath(FDT_DATA, FontManager::getDrawFontFile()), 22);
  m_fontBig      = getFontManager(XMFS::FullPath(FDT_DATA, FontManager::getDrawFontFile()), 60);
  m_fontMonospace= getFontManager(XMFS::FullPath(FDT_DATA, FontManager::getMonospaceFontFile()), 12, 7);

  LogInfo("DrawLibGLES2: WebGL2 context created (%ux%u)", nW, nH);
}

void DrawLibGLES2::unInit() {
  if (m_fontSmall)    { delete m_fontSmall;    m_fontSmall    = nullptr; }
  if (m_fontMedium)   { delete m_fontMedium;   m_fontMedium   = nullptr; }
  if (m_fontBig)      { delete m_fontBig;      m_fontBig      = nullptr; }
  if (m_fontMonospace){ delete m_fontMonospace; m_fontMonospace= nullptr; }
  if (m_menuCamera)   { delete m_menuCamera;   m_menuCamera   = nullptr; }
  if (m_vbo)          { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
  if (m_prog)         { glDeleteProgram(m_prog); m_prog = 0; }
  if (m_glContext)    { SDL_GL_DeleteContext(m_glContext); m_glContext = nullptr; }
  if (m_window)       { SDL_DestroyWindow(m_window); m_window = nullptr; }
}

/* -------------------------------------------------------------------------
   Internal helpers
   ------------------------------------------------------------------------- */

void DrawLibGLES2::_recompute_mvp() {
  mat4_multiply(m_mvp, m_proj, m_model);
}

void DrawLibGLES2::_flush_batch() {
  if (m_batch.empty()) return;

  glUseProgram(m_prog);
  glUniformMatrix4fv(m_loc_mvp, 1, GL_FALSE, m_mvp);
  glUniform1i(m_loc_tex, 0);

  if (m_texture) {
    glBindTexture(GL_TEXTURE_2D, m_texture->nID);
    glUniform1i(m_loc_use_tex, 1);
  } else {
    glUniform1i(m_loc_use_tex, 0);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               (GLsizeiptr)(m_batch.size() * sizeof(Vertex2D)),
               m_batch.data(), GL_DYNAMIC_DRAW);

  glEnableVertexAttribArray(m_loc_pos);
  glVertexAttribPointer(m_loc_pos, 2, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex2D), (void *)offsetof(Vertex2D, x));
  glEnableVertexAttribArray(m_loc_uv);
  glVertexAttribPointer(m_loc_uv,  2, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex2D), (void *)offsetof(Vertex2D, u));
  glEnableVertexAttribArray(m_loc_color);
  glVertexAttribPointer(m_loc_color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                        sizeof(Vertex2D), (void *)offsetof(Vertex2D, r));

  GLenum gl_mode;
  switch (m_drawMode) {
    case DRAW_MODE_LINE_LOOP:  gl_mode = GL_LINE_LOOP;  break;
    case DRAW_MODE_LINE_STRIP: gl_mode = GL_LINE_STRIP; break;
    default:                   gl_mode = GL_TRIANGLE_FAN; break;
  }
  glDrawArrays(gl_mode, 0, (GLsizei)m_batch.size());

  glDisableVertexAttribArray(m_loc_pos);
  glDisableVertexAttribArray(m_loc_uv);
  glDisableVertexAttribArray(m_loc_color);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_batch.clear();
}

/* -------------------------------------------------------------------------
   DrawLib interface implementation
   ------------------------------------------------------------------------- */

void DrawLibGLES2::clearGraphics() {
  glClear(GL_COLOR_BUFFER_BIT);
  mat4_identity(m_model);
  _recompute_mvp();
}

void DrawLibGLES2::flushGraphics() {
  _flush_batch();
  SDL_GL_SwapWindow(m_window);
}

void DrawLibGLES2::startDraw(DrawMode mode) {
  _flush_batch(); /* flush any pending geometry first */
  m_drawMode = mode;
}

void DrawLibGLES2::endDraw() {
  _flush_batch();
  if (m_blendMode != BLEND_MODE_NONE) glDisable(GL_BLEND);
  m_texture   = nullptr;
  m_blendMode = BLEND_MODE_NONE;
}

void DrawLibGLES2::endDrawKeepProperties() {
  _flush_batch();
}

void DrawLibGLES2::removePropertiesAfterEnd() {
  _flush_batch();
  m_texture   = nullptr;
  m_blendMode = BLEND_MODE_NONE;
  if (m_blendMode != BLEND_MODE_NONE) glDisable(GL_BLEND);
}

void DrawLibGLES2::glVertexSP(float x, float y) {
  /* SP = Screen Position: y=0 is TOP of screen in xmoto coords */
  m_batch.push_back({x, (float)m_nDispHeight - y,
                     m_cur_u, m_cur_v,
                     m_cur_r, m_cur_g, m_cur_b, m_cur_a});
}

void DrawLibGLES2::glVertex(float x, float y) {
  m_batch.push_back({x, y,
                     m_cur_u, m_cur_v,
                     m_cur_r, m_cur_g, m_cur_b, m_cur_a});
}

void DrawLibGLES2::glTexCoord(float u, float v) {
  m_cur_u = u; m_cur_v = v;
}

void DrawLibGLES2::setColor(Color color) {
  m_cur_r = GET_RED(color);
  m_cur_g = GET_GREEN(color);
  m_cur_b = GET_BLUE(color);
  m_cur_a = GET_ALPHA(color);
}

void DrawLibGLES2::setTexture(Texture *tex, BlendMode bm) {
  if (tex != m_texture || bm != m_blendMode) {
    _flush_batch(); /* flush before state change */
    m_texture   = tex;
  }
  setBlendMode(bm);
}

void DrawLibGLES2::setBlendMode(BlendMode bm) {
  if (bm != m_blendMode) _flush_batch();
  if (bm != BLEND_MODE_NONE) {
    glEnable(GL_BLEND);
    if (bm == BLEND_MODE_A)
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else
      glBlendFunc(GL_ONE, GL_ONE);
  } else {
    glDisable(GL_BLEND);
  }
  m_blendMode = bm;
}

void DrawLibGLES2::setClipRect(int x, int y, unsigned int w, unsigned int h) {
  _flush_batch();
  m_nLScissorX = x; m_nLScissorY = y;
  m_nLScissorW = w; m_nLScissorH = h;
  /* m_renderSurf->upright().y converts screen-y (top=0) to GL-y (bottom=0).
     Fall back to m_nDispHeight when m_renderSurf is not yet set.           */
  int surfH = m_renderSurf ? m_renderSurf->upright().y : (int)m_nDispHeight;
  glScissor(x, surfH - (y + (int)h), (int)w, (int)h);
}

void DrawLibGLES2::setClipRect(SDL_Rect *r) {
  if (r) setClipRect(r->x, r->y, r->w, r->h);
}

void DrawLibGLES2::getClipRect(int *px, int *py, int *pw, int *ph) {
  if (px) *px = m_nLScissorX;
  if (py) *py = m_nLScissorY;
  if (pw) *pw = m_nLScissorW;
  if (ph) *ph = m_nLScissorH;
}

void DrawLibGLES2::setScale(float x, float y) {
  _flush_batch();
  mat4_scale(m_model, x, y);
  _recompute_mvp();
}

void DrawLibGLES2::setTranslate(float x, float y) {
  _flush_batch();
  mat4_translate(m_model, x, y);
  _recompute_mvp();
}

void DrawLibGLES2::setMirrorY() {
  _flush_batch();
  mat4_scale(m_model, -1.f, 1.f);
  _recompute_mvp();
}

void DrawLibGLES2::setRotateZ(float angle) {
  if (angle == 0.f) return;
  _flush_batch();
  mat4_rotate_z(m_model, angle);
  _recompute_mvp();
}

void DrawLibGLES2::setLineWidth(float) {
  glLineWidth(1.f); /* WebGL2 only supports width=1 */
}

void DrawLibGLES2::setCameraDimensionality(CameraDimension dim) {
  _flush_batch();
  /* Guard: m_renderSurf is null before setRenderSurface() is first called.
     Fall back to the full display viewport so glViewport always succeeds.   */
  if (m_renderSurf) {
    glViewport(m_renderSurf->downleft().x, m_renderSurf->downleft().y,
               m_renderSurf->size().x,     m_renderSurf->size().y);
  } else {
    glViewport(0, 0, (int)m_nDispWidth, (int)m_nDispHeight);
  }

  if (dim == CAMERA_2D) {
    /* Screen-pixel projection: map [0,W]×[0,H] → [-1,1] clip space.
       glVertexSP() feeds pixel coords (with y flipped to OpenGL orientation).*/
    int w = m_renderSurf ? m_renderSurf->size().x : (int)m_nDispWidth;
    int h = m_renderSurf ? m_renderSurf->size().y : (int)m_nDispHeight;
    mat4_ortho(m_proj, 0.f, (float)w, 0.f, (float)h, -1.f, 1.f);
  } else {
    /* Game-world projection: identity, same as desktop GL (CAMERA_3D).
       The renderer then applies scale + translate via setScale/setTranslate
       so that world coords map directly to NDC.  glVertex() feeds raw
       game-world coordinates which the model matrix transforms to NDC. */
    mat4_identity(m_proj);
  }

  mat4_identity(m_model);
  _recompute_mvp();
}

void DrawLibGLES2::resetGraphics() {
  _flush_batch();
  mat4_identity(m_model);
  _recompute_mvp();
}

FontManager *DrawLibGLES2::getFontManager(const std::string &file,
                                           unsigned int size,
                                           unsigned int fixedW) {
  return new GLES2FontManager(this, file, size, fixedW);
}

Img *DrawLibGLES2::grabScreen(int) {
  return nullptr; /* not needed on web */
}

bool DrawLibGLES2::isExtensionSupported(std::string) {
  return false;
}

/* =========================================================================
   GLES2FontManager::printStringGradOne
   ========================================================================= */

void GLES2FontManager::printStringGradOne(DrawLib *dl, FontGlyph *fg,
    int ix, int iy,
    Color c1, Color c2, Color c3, Color c4, float perCentered) {
  GLES2FontGlyph *g = static_cast<GLES2FontGlyph *>(fg);
  const std::string &val = g->m_value;
  if (val.empty()) return;

  DrawLibGLES2 *gles = static_cast<DrawLibGLES2 *>(dl);

  uint8_t r1 = GET_RED(c1), g1 = GET_GREEN(c1), b1 = GET_BLUE(c1), a1 = GET_ALPHA(c1);
  uint8_t r2 = GET_RED(c2), g2 = GET_GREEN(c2), b2 = GET_BLUE(c2), a2 = GET_ALPHA(c2);
  uint8_t r3 = GET_RED(c3), g3 = GET_GREEN(c3), b3 = GET_BLUE(c3), a3 = GET_ALPHA(c3);
  uint8_t r4 = GET_RED(c4), g4 = GET_GREEN(c4), b4 = GET_BLUE(c4), a4 = GET_ALPHA(c4);

  gles->setBlendMode(BLEND_MODE_A);

  unsigned int longest = getLongestLineSize(val);
  unsigned int curLine = getLongestLineSize(val, 0, 1);

  int vx = ix + (int)((longest - curLine) / 2 * (perCentered + 1.0));
  /* Guard: getRenderSurface() returns null if setRenderSurface() has not yet
     been called (e.g., during early state transitions).  Fall back to the
     display height so the vertical text position is still correct.         */
  int surfH = dl->getRenderSurface() ? dl->getRenderSurface()->size().y
                                     : (int)dl->getDispHeight();
  int vy = -iy + surfH - (int)g->firstLineH();
  unsigned int lineH = 0;
  bool prevNL = false;

  unsigned int n = 0;
  std::string ch;
  while (n < val.size()) {
    utf8::getNextChar(val, n, ch);
    if (ch == "\n") {
      curLine = getLongestLineSize(val, n, 1);
      vx = ix + (int)((longest - curLine) / 2 * (perCentered + 1.0));
      vy -= (int)(lineH + UTF8_INTERLINE_SPACE);
      if (prevNL) {
        auto *sp = m_letters[" "];
        if (sp) { vy -= (int)sp->realHeight(); }
      }
      prevNL = true; lineH = 0;
    } else {
      prevNL = false;
      GLES2FontGlyphLetter *l = m_letters[ch];
      if (!l) continue;
      if (l->realHeight() > lineH) lineH = l->realHeight();

      /* Draw a textured quad for this glyph */
      gles->m_texture = nullptr; /* force texture bind below */
      glBindTexture(GL_TEXTURE_2D, l->m_texID);
      glUseProgram(gles->m_prog);
      glUniform1i(gles->m_loc_use_tex, 1);
      glUniform1i(gles->m_loc_tex, 0);
      glUniformMatrix4fv(gles->m_loc_mvp, 1, GL_FALSE, gles->m_mvp);

      float x0 = (float)vx,              y0 = (float)vy;
      float x1 = (float)(vx + (int)l->m_drawW), y1 = (float)(vy + (int)l->m_drawH);
      float ux = l->m_u.x, uy = l->m_u.y, vvx = l->m_v.x, vvy = l->m_v.y;

      /* SDL TTF surfaces are stored top-to-bottom; OpenGL/GLES2 UV (0,0) is
         at the BOTTOM-LEFT of the texture (bottom of the SDL image = bottom of
         the character).  Match the desktop GLFontManager convention: the
         GL-bottom vertex uses UV v=vvy (SDL bottom = char bottom) and the
         GL-top vertex uses UV v=uy=0 (SDL top = char top).               */
      Vertex2D q[4] = {
        {x0, y0, ux,  vvy, r1, g1, b1, a1},   /* GL bottom-left  → char bottom-left  */
        {x1, y0, vvx, vvy, r2, g2, b2, a2},   /* GL bottom-right → char bottom-right */
        {x1, y1, vvx, uy,  r4, g4, b4, a4},   /* GL top-right    → char top-right    */
        {x0, y1, ux,  uy,  r3, g3, b3, a3},   /* GL top-left     → char top-left     */
      };

      glBindBuffer(GL_ARRAY_BUFFER, gles->m_vbo);
      glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vertex2D), q, GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(gles->m_loc_pos);
      glVertexAttribPointer(gles->m_loc_pos, 2, GL_FLOAT, GL_FALSE,
                            sizeof(Vertex2D), (void*)offsetof(Vertex2D, x));
      glEnableVertexAttribArray(gles->m_loc_uv);
      glVertexAttribPointer(gles->m_loc_uv, 2, GL_FLOAT, GL_FALSE,
                            sizeof(Vertex2D), (void*)offsetof(Vertex2D, u));
      glEnableVertexAttribArray(gles->m_loc_color);
      glVertexAttribPointer(gles->m_loc_color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                            sizeof(Vertex2D), (void*)offsetof(Vertex2D, r));
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
      glDisableVertexAttribArray(gles->m_loc_pos);
      glDisableVertexAttribArray(gles->m_loc_uv);
      glDisableVertexAttribArray(gles->m_loc_color);
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      vx += (int)(l->realWidth() + UTF8_INTERCHAR_SPACE);
    }
  }
  glDisable(GL_BLEND);
}

#endif /* __EMSCRIPTEN__ */

