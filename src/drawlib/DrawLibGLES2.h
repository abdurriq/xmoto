#pragma once
/*
 * DrawLibGLES2.h — X-Moto GLES2 / WebGL2 rendering backend (Phase 5).
 */

#ifdef __EMSCRIPTEN__

#include "DrawLib.h"
#include <GLES2/gl2.h>
#include <vector>
#include <cstdint>

struct Vertex2D {
  float   x, y;
  float   u, v;
  uint8_t r, g, b, a;
};

class DrawLibGLES2 : public DrawLib {
  friend class GLES2FontManager;

public:
  DrawLibGLES2();
  virtual ~DrawLibGLES2();

  virtual void init(unsigned int nDispWidth,
                    unsigned int nDispHeight,
                    bool bWindowed) override;
  virtual void unInit() override;

  virtual void glVertexSP(float x, float y) override;
  virtual void glVertex(float x, float y) override;
  virtual void glTexCoord(float u, float v) override;
  virtual void setColor(Color color) override;
  virtual void setTexture(Texture *texture, BlendMode blendMode) override;
  virtual void setBlendMode(BlendMode blendMode) override;

  virtual void setClipRect(int x, int y, unsigned int w, unsigned int h) override;
  virtual void setClipRect(SDL_Rect *i_clip_rect) override;
  virtual void setScale(float x, float y) override;
  virtual void setTranslate(float x, float y) override;
  virtual void setMirrorY() override;
  virtual void setRotateZ(float i_angle) override;
  virtual void setLineWidth(float width) override;
  virtual void getMVP(float *out16) const override;
  virtual void pushTransform() override;
  virtual void popTransform() override;

  virtual void getClipRect(int *o_px, int *o_py,
                           int *o_pnWidth, int *o_pnHeight) override;

  virtual void startDraw(DrawMode mode) override;
  virtual void endDraw() override;
  virtual void endDrawKeepProperties() override;
  virtual void removePropertiesAfterEnd() override;

  virtual void clearGraphics() override;
  virtual void resetGraphics() override;
  virtual void flushGraphics() override;

  virtual FontManager *getFontManager(const std::string &i_fontFile,
                                      unsigned int i_fontSize,
                                      unsigned int i_fixedFontSize = 0) override;
  virtual Img *grabScreen(int i_reduce = 1) override;
  virtual bool isExtensionSupported(std::string Ext) override;

  virtual void setCameraDimensionality(CameraDimension dimension) override;

private:
  SDL_GLContext m_glContext;

  /* shader */
  GLuint m_prog;
  GLint  m_loc_mvp, m_loc_use_tex, m_loc_tex;
  GLint  m_loc_pos, m_loc_uv, m_loc_color;

  /* vertex batch */
  GLuint                m_vbo;
  std::vector<Vertex2D> m_batch;
  DrawMode              m_drawMode;

  /* matrices (column-major 4x4) */
  float m_proj[16];
  float m_model[16];
  float m_mvp[16];
  float m_saved_proj[16];   /* scratch save slot for pushTransform() */
  float m_saved_model[16];
  void  _recompute_mvp();

  /* current draw state */
  uint8_t m_cur_r, m_cur_g, m_cur_b, m_cur_a;
  float   m_cur_u, m_cur_v;

  void _flush_batch();
};

#endif /* __EMSCRIPTEN__ */
