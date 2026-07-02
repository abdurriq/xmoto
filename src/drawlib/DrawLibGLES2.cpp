/*
 * DrawLibGLES2.cpp — Emscripten/WebGL2 rendering back-end.
 *
 * Phase 2 stub: every method is a no-op or minimal implementation so that
 * the project compiles and links cleanly (Checkpoint 2).  Phase 5 replaces
 * these bodies with a real GLES2/WebGL2 renderer.
 */

#ifdef __EMSCRIPTEN__

#include "DrawLibGLES2.h"
#include "common/VFileIO.h"
#include "helpers/Log.h"
#include "helpers/RenderSurface.h"
#include "xmscene/Camera.h"
#include "common/Image.h"

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

DrawLibGLES2::DrawLibGLES2() : DrawLib(), m_glContext(nullptr) {}

DrawLibGLES2::~DrawLibGLES2() {}

void DrawLibGLES2::init(unsigned int nDispWidth,
                        unsigned int nDispHeight,
                        bool bWindowed) {
  DrawLib::init(nDispWidth, nDispHeight, bWindowed);

  m_nDispWidth  = nDispWidth;
  m_nDispHeight = nDispHeight;
  m_bWindowed   = bWindowed;

  /* Request GLES2 context */
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  int nFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;

  std::string title = "X-Moto (Web)";
  m_window = SDL_CreateWindow(title.c_str(),
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              m_nDispWidth, m_nDispHeight,
                              nFlags);
  if (!m_window) {
    throw Exception(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
  }

  m_glContext = SDL_GL_CreateContext(m_window);
  if (!m_glContext) {
    throw Exception(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
  }

  m_menuCamera = new Camera(Vector2i(0, 0),
                            Vector2i(m_nDispWidth, m_nDispHeight));
  m_menuCamera->setCamera2d();

  LogInfo("DrawLibGLES2: WebGL2 context created (%ux%u)", nDispWidth, nDispHeight);
}

void DrawLibGLES2::unInit() {
  if (m_menuCamera) { delete m_menuCamera; m_menuCamera = nullptr; }
  if (m_glContext)  { SDL_GL_DeleteContext(m_glContext); m_glContext = nullptr; }
  if (m_window)     { SDL_DestroyWindow(m_window); m_window = nullptr; }
}

void DrawLibGLES2::setCameraDimensionality(CameraDimension) {}
void DrawLibGLES2::glVertexSP(float, float) {}
void DrawLibGLES2::glVertex(float, float) {}
void DrawLibGLES2::glTexCoord(float, float) {}
void DrawLibGLES2::setColor(Color) {}
void DrawLibGLES2::setTexture(Texture *, BlendMode) {}
void DrawLibGLES2::setBlendMode(BlendMode) {}
void DrawLibGLES2::setClipRect(int, int, unsigned int, unsigned int) {}
void DrawLibGLES2::setClipRect(SDL_Rect *) {}
void DrawLibGLES2::setScale(float, float) {}
void DrawLibGLES2::setTranslate(float, float) {}
void DrawLibGLES2::setMirrorY() {}
void DrawLibGLES2::setRotateZ(float) {}
void DrawLibGLES2::setLineWidth(float) {}

void DrawLibGLES2::getClipRect(int *px, int *py, int *pw, int *ph) {
  if (px) *px = 0; if (py) *py = 0;
  if (pw) *pw = m_nDispWidth; if (ph) *ph = m_nDispHeight;
}

void DrawLibGLES2::startDraw(DrawMode) {}
void DrawLibGLES2::endDraw() {}
void DrawLibGLES2::endDrawKeepProperties() {}
void DrawLibGLES2::removePropertiesAfterEnd() {}
void DrawLibGLES2::clearGraphics() { glClear(GL_COLOR_BUFFER_BIT); }
void DrawLibGLES2::flushGraphics() { SDL_GL_SwapWindow(m_window); }

FontManager *DrawLibGLES2::getFontManager(const std::string &,
                                          unsigned int,
                                          unsigned int) {
  /* Phase 5 will implement TTF font rendering via GLES2 */
  throw Exception("DrawLibGLES2::getFontManager not yet implemented (Phase 5)");
}

Img *DrawLibGLES2::grabScreen(int) {
  return nullptr; /* screenshots not supported in Phase 2 stub */
}

bool DrawLibGLES2::isExtensionSupported(std::string) {
  return false;
}

#endif /* __EMSCRIPTEN__ */
