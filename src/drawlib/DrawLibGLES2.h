#pragma once
/*
 * DrawLibGLES2.h — Emscripten/WebGL2 rendering back-end.
 *
 * Phase 2: compile-and-link stub.  All methods are implemented in
 * DrawLibGLES2.cpp with no-op or minimal bodies so that the project links
 * cleanly (Checkpoint 2).  The real GLES2 renderer is written in Phase 5.
 */

#include "DrawLib.h"

class DrawLibGLES2 : public DrawLib {
public:
  DrawLibGLES2();
  virtual ~DrawLibGLES2();

  virtual void init(unsigned int nDispWidth,
                    unsigned int nDispHeight,
                    bool bWindowed) override;
  virtual void unInit() override;

  virtual void glVertexSP(float x, float y) override;
  virtual void glVertex(float x, float y) override;
  virtual void glTexCoord(float x, float y) override;
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

  virtual void getClipRect(int *o_px, int *o_py,
                           int *o_pnWidth, int *o_pnHeight) override;

  virtual void startDraw(DrawMode mode) override;
  virtual void endDraw() override;
  virtual void endDrawKeepProperties() override;
  virtual void removePropertiesAfterEnd() override;

  virtual void clearGraphics() override;
  virtual void flushGraphics() override;

  virtual FontManager *getFontManager(const std::string &i_fontFile,
                                      unsigned int i_fontSize,
                                      unsigned int i_fixedFontSize = 0) override;
  virtual Img *grabScreen(int i_reduce = 1) override;
  virtual bool isExtensionSupported(std::string Ext) override;

  virtual void setCameraDimensionality(CameraDimension dimension) override;

private:
  SDL_GLContext m_glContext;
};
