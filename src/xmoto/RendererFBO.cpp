/*=============================================================================
XMOTO

This file is part of XMOTO.

XMOTO is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

XMOTO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XMOTO; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

/*
 *  In-game rendering (FBO stuff)
 */
#include "GameText.h"
#include "Renderer.h"
#include "common/VFileIO.h"
#include "common/VXml.h"
#include "drawlib/DrawLib.h"
#include "helpers/Log.h"
#include "helpers/RenderSurface.h"

#ifdef ENABLE_OPENGL
#include "drawlib/DrawLibOpenGL.h"
#endif

/*===========================================================================
Init and clean up
===========================================================================*/
void SFXOverlay::init(DrawLib *i_drawLib,
                      RenderSurface *i_screen,
                      unsigned int nWidth,
                      unsigned int nHeight) {
#ifdef ENABLE_OPENGL
  m_drawLib = i_drawLib;
  m_screen = i_screen;
  m_nOverlayWidth = nWidth;
  m_nOverlayHeight = nHeight;

  if (m_drawLib->useFBOs()) {
    /* Create overlay */
    glEnable(GL_TEXTURE_2D);

    glGenFramebuffersEXT(1, &m_FrameBufferID);
    glGenTextures(1, &m_DynamicTextureID);
    glBindTexture(GL_TEXTURE_2D, m_DynamicTextureID);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 nWidth,
                 nHeight,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glDisable(GL_TEXTURE_2D);

    /* Shaders? */
    m_bUseShaders = m_drawLib->useShaders();

    if (m_bUseShaders) {
      m_VertShaderID = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
      m_FragShaderID = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

      if (!_SetShaderSource(m_VertShaderID, "SFXOverlay.vert") ||
          !_SetShaderSource(m_FragShaderID, "SFXOverlay.frag"))
        m_bUseShaders = false;
      else {
        /* Source loaded good... Now create the program */
        m_ProgramID = glCreateProgramObjectARB();

        /* Attach our shaders to it */
        glAttachObjectARB(m_ProgramID, m_VertShaderID);
        glAttachObjectARB(m_ProgramID, m_FragShaderID);

        /* Link it */
        glLinkProgramARB(m_ProgramID);

        int nStatus = 0;
        glGetObjectParameterivARB(
          m_ProgramID, GL_OBJECT_LINK_STATUS_ARB, (GLint *)&nStatus);
        if (!nStatus) {
          LogError("-- Failed to link SFXOverlay shader program --");

          /* Retrieve info-log */
          int nInfoLogLen = 0;
          glGetObjectParameterivARB(
            m_ProgramID, GL_OBJECT_INFO_LOG_LENGTH_ARB, (GLint *)&nInfoLogLen);
          char *pcInfoLog = new char[nInfoLogLen];
          int nCharsWritten = 0;
          glGetInfoLogARB(
            m_ProgramID, nInfoLogLen, (GLsizei *)&nCharsWritten, pcInfoLog);
          LogInfo(pcInfoLog);
          delete[] pcInfoLog;

          m_bUseShaders = false;
        } else {
          /* Linked OK! */
        }
      }
    }
  }
#endif
}

void SFXOverlay::cleanUp(void) {
#ifdef ENABLE_OPENGL
  if (m_drawLib != NULL) {
    if (m_drawLib->useFBOs()) {
      /* Delete stuff */
      glDeleteTextures(1, &m_DynamicTextureID);
      glDeleteFramebuffersEXT(1, &m_FrameBufferID);
    }
  }
#endif
}

/*===========================================================================
Start/stop rendering
===========================================================================*/
void SFXOverlay::beginRendering(void) {
#ifdef ENABLE_OPENGL
  if (m_drawLib->useFBOs()) {
    glEnable(GL_TEXTURE_2D);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FrameBufferID);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D,
                              m_DynamicTextureID,
                              0);

    glDisable(GL_TEXTURE_2D);

    glViewport(0, 0, m_nOverlayWidth, m_nOverlayHeight);
  }
#endif
}

void SFXOverlay::endRendering(void) {
#ifdef ENABLE_OPENGL
  if (m_drawLib->useFBOs()) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glViewport(m_screen->downleft().x,
               m_screen->downleft().y,
               m_screen->size().x,
               m_screen->size().y);
  }
#endif
}

#ifdef ENABLE_OPENGL
/*===========================================================================
Load shader source
===========================================================================*/
char **SFXOverlay::_LoadShaderSource(const std::string &File,
                                     unsigned int *pnNumLines) {
  FileHandle *pfh = XMFS::openIFile(FDT_DATA, std::string("Shaders/") + File);
  if (pfh != NULL) {
    /* Load lines */
    std::string Line;
    std::vector<std::string> Lines;
    while (XMFS::readNextLine(pfh, Line)) {
      Lines.push_back(Line);
    }
    XMFS::closeFile(pfh);

    /* Convert line array into something OpenGL will eat */
    char **ppc = new char *[Lines.size()];
    for (unsigned int i = 0; i < Lines.size(); i++) {
      char *pc = new char[Lines[i].length() + 1];
      strcpy(pc, Lines[i].c_str());
      ppc[i] = pc;
    }
    *pnNumLines = Lines.size();

    return ppc;
  }
  /* Nothing! */
  return NULL;
}

bool SFXOverlay::_SetShaderSource(GLhandleARB ShaderID,
                                  const std::string &File) {
  /* Try loading file */
  unsigned int nNumLines;
  char **ppc = _LoadShaderSource(File, &nNumLines);
  if (ppc == NULL)
    return false; /* no shader */

  /* Pass it to OpenGL */
  glShaderSourceARB(ShaderID, nNumLines, (const GLcharARB **)ppc, NULL);

  /* Compile it! */
  glCompileShaderARB(ShaderID);
  int nStatus = 0;
  glGetObjectParameterivARB(
    ShaderID, GL_OBJECT_COMPILE_STATUS_ARB, (GLint *)&nStatus);
  if (!nStatus) {
    _FreeShaderSource(ppc, nNumLines);
    LogError("-- Failed to compile shader: %s --", File.c_str());

    /* Retrieve info-log */
    int nInfoLogLen = 0;
    glGetObjectParameterivARB(
      ShaderID, GL_OBJECT_INFO_LOG_LENGTH_ARB, (GLint *)&nInfoLogLen);
    char *pcInfoLog = new char[nInfoLogLen];
    int nCharsWritten = 0;
    glGetInfoLogARB(
      ShaderID, nInfoLogLen, (GLsizei *)&nCharsWritten, pcInfoLog);
    LogInfo(pcInfoLog);
    delete[] pcInfoLog;

    return false;
  }

  /* Free source */
  _FreeShaderSource(ppc, nNumLines);
  return true;
}

void SFXOverlay::_FreeShaderSource(char **ppc, unsigned int nNumLines) {
  for (unsigned int i = 0; i < nNumLines; i++) {
    delete[] ppc[i];
  }
  delete[] ppc;
}
#endif

/*===========================================================================
Fading...
===========================================================================*/
void SFXOverlay::fade(float f, unsigned int i_frameNumber) {
  if (i_frameNumber < 5) {
    f = 1.0; // fade 100% at the beginning of the ghost rendering to avoid bad
    // visual effects
  }

#ifdef __EMSCRIPTEN__
  if (m_drawLib->useFBOs()) {
    /* FBO is active (512x512 viewport). m_proj=identity (CAMERA_3D).
       Reset m_model to identity so MVP=identity, then draw a full-viewport
       black quad in NDC space (-1,-1) to (1,1) to darken the ghost trail. */
    m_drawLib->pushTransform();
    m_drawLib->resetGraphics();  /* m_model=identity; m_proj stays identity */
    m_drawLib->setBlendMode(BLEND_MODE_A);
    m_drawLib->startDraw(DRAW_MODE_POLYGON);
    m_drawLib->setColorRGBA(0, 0, 0, (int)(f * 255));
    m_drawLib->glVertex(-1.0f, -1.0f);
    m_drawLib->glVertex( 1.0f, -1.0f);
    m_drawLib->glVertex( 1.0f,  1.0f);
    m_drawLib->glVertex(-1.0f,  1.0f);
    m_drawLib->endDraw();
    m_drawLib->popTransform();
  }
#elif defined(ENABLE_OPENGL)
  if (m_drawLib->useFBOs()) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_drawLib->startDraw(DRAW_MODE_POLYGON);
    m_drawLib->setColorRGBA(0, 0, 0, (int)(f * 255));
    glVertex2f(0, 0);
    glVertex2f(1, 0);
    glVertex2f(1, 1);
    glVertex2f(0, 1);
    m_drawLib->endDraw();
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
  }
#endif
}

void SFXOverlay::present(void) {
#ifdef __EMSCRIPTEN__
  /* GLES2: route the FBO texture through the DrawLib API.
     Wrap the FBO texture ID in a temporary Texture so setTexture() picks it
     up; then render it full-screen in CAMERA_2D (pixel) space.
     Coords (0,0)→(W,H) with ortho(0,W,0,H): y=0 is bottom → UV(0,0)=FBO
     bottom-left and UV(1,1)=FBO top-right, both correct for GL convention. */
  if (m_drawLib->useFBOs()) {
    Texture fboTex = {};              /* zero-initialise all fields */
    fboTex.nID = m_DynamicTextureID;

    int W = m_screen->getDispWidth();
    int H = m_screen->getDispHeight();

    m_drawLib->pushTransform();
    m_drawLib->setCameraDimensionality(CAMERA_2D);
    m_drawLib->setTexture(&fboTex, BLEND_MODE_B);
    m_drawLib->drawImageTextureSet(
      Vector2f(0.0f,   0.0f),
      Vector2f((float)W, 0.0f),
      Vector2f((float)W, (float)H),
      Vector2f(0.0f,   (float)H),
      MAKE_COLOR(255, 255, 255, 255));
    m_drawLib->setTexture(nullptr, BLEND_MODE_NONE);
    m_drawLib->popTransform();
  }
#elif defined(ENABLE_OPENGL)
  if (m_drawLib->useFBOs()) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, m_screen->getDispWidth(), 0, m_screen->getDispHeight(), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    if (m_bUseShaders)
      glUseProgramObjectARB(m_ProgramID);

    m_drawLib->setBlendMode(BLEND_MODE_B);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_DynamicTextureID);

    m_drawLib->drawImageTextureSet(
      Vector2f(0.0, 0.0),
      Vector2f(m_screen->getDispWidth(), 0.0),
      Vector2f(m_screen->getDispWidth(), m_screen->getDispHeight()),
      Vector2f(0.0, m_screen->getDispHeight()),
      MAKE_COLOR(255, 255, 255, 255));
    glDisable(GL_TEXTURE_2D);

    if (m_bUseShaders)
      glUseProgramObjectARB(0);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
  }
#endif
}
