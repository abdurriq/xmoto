#ifdef ENABLE_OPENGL

#ifdef __EMSCRIPTEN__
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>

/* -----------------------------------------------------------------------
 * Checkpoint-2 compatibility layer for Emscripten / GLES2 / WebGL2.
 * Phase 5 removes these and adds proper GLES2 rendering code.
 * --------------------------------------------------------------------- */

/* Fixed-function matrix stack -> no-ops */
#  define GL_PROJECTION          0x1701
#  define GL_MODELVIEW           0x1700
#  define GL_MODELVIEW_MATRIX    0x0BA6
#  define GL_PROJECTION_MATRIX   0x0BA7
#  define glGetFloatv_noop(pname, params)  ((void)0)
#  define glMatrixMode(x)        ((void)0)
#  define glLoadIdentity()       ((void)0)
#  define glPushMatrix()         ((void)0)
#  define glPopMatrix()          ((void)0)
#  define glOrtho(l,r,b,t,n,f)  ((void)0)
#  define glFrustum(...)         ((void)0)
#  define glTranslatef(x,y,z)   ((void)0)
#  define glRotatef(a,x,y,z)    ((void)0)
#  define glScalef(x,y,z)       ((void)0)

/* Fixed-function immediate mode -> no-ops */
#  define GL_POLYGON             0x0009
#  define GL_QUADS               0x0007
#  define glBegin(x)             ((void)0)
#  define glEnd()                ((void)0)
#  define glVertex2f(x,y)        ((void)0)
#  define glVertex2i(x,y)        ((void)0)
#  define glTexCoord2f(x,y)      ((void)0)
#  define glColor4ub(r,g,b,a)   ((void)0)
#  define glColor4f(r,g,b,a)    ((void)0)
#  define glColor3f(r,g,b)      ((void)0)

/* Fixed-function client state -> no-ops */
#  define GL_VERTEX_ARRAY        0x8074
#  define GL_TEXTURE_COORD_ARRAY 0x8078
#  define GL_COLOR_ARRAY         0x8076
#  define GL_NORMAL_ARRAY        0x8075
#  define glEnableClientState(x)  ((void)0)
#  define glDisableClientState(x) ((void)0)
#  define glVertexPointer(...)    ((void)0)
#  define glTexCoordPointer(...)  ((void)0)
#  define glColorPointer(...)     ((void)0)
#  define glNormalPointer(...)    ((void)0)

/* Fixed-function alpha test -> no-op */
#  define GL_ALPHA_TEST           0x0BC0
#  define glAlphaFunc(f,r)        ((void)0)
#  define glReadBuffer(x)         ((void)0)

/* ARB VBO compat */
#  define glGenBuffersARB             glGenBuffers
#  define glDeleteBuffersARB          glDeleteBuffers
#  define glBindBufferARB             glBindBuffer
#  define glBufferDataARB             glBufferData
#  define GL_ARRAY_BUFFER_ARB         GL_ARRAY_BUFFER
#  define GL_STATIC_DRAW_ARB          GL_STATIC_DRAW
#  define GL_ELEMENT_ARRAY_BUFFER_ARB GL_ELEMENT_ARRAY_BUFFER

/* ARB shader compat */
#  define GLhandleARB                    GLuint
#  define GLcharARB                      GLchar
#  define GL_VERTEX_SHADER_ARB           GL_VERTEX_SHADER
#  define GL_FRAGMENT_SHADER_ARB         GL_FRAGMENT_SHADER
#  define GL_OBJECT_COMPILE_STATUS_ARB   GL_COMPILE_STATUS
#  define GL_OBJECT_LINK_STATUS_ARB      GL_LINK_STATUS
#  define GL_OBJECT_INFO_LOG_LENGTH_ARB  GL_INFO_LOG_LENGTH
#  define glCreateShaderObjectARB        glCreateShader
#  define glCreateProgramObjectARB       glCreateProgram
#  define glAttachObjectARB              glAttachShader
#  define glLinkProgramARB               glLinkProgram
#  define glUseProgramObjectARB          glUseProgram
#  define glShaderSourceARB              glShaderSource
#  define glCompileShaderARB             glCompileShader
#  define glGetObjectParameterivARB(o,p,v) glGetShaderiv(o,p,v)
#  define glGetInfoLogARB(o,m,l,b)         glGetShaderInfoLog(o,m,l,b)

/* FBO compat */
#  define GL_FRAMEBUFFER_EXT           GL_FRAMEBUFFER
#  define GL_COLOR_ATTACHMENT0_EXT     GL_COLOR_ATTACHMENT0
#  define glGenFramebuffersEXT         glGenFramebuffers
#  define glDeleteFramebuffersEXT      glDeleteFramebuffers
#  define glBindFramebufferEXT         glBindFramebuffer
#  define glFramebufferTexture2DEXT    glFramebufferTexture2D

#else /* !__EMSCRIPTEN__ — desktop OpenGL */
#  include <glad.h>

/* Pull in OpenGL headers */
/* following scissored from SDL_opengl.h */
#  if defined(__APPLE__) || defined(HAVE_APPLE_OPENGL_FRAMEWORK)
#    include <OpenGL/gl.h>  /* Header File For The OpenGL Library */
#    include <OpenGL/glu.h> /* Header File For The GLU Library */
#  elif defined(__MACOS__)
#    include <gl.h>         /* Header File For The OpenGL Library */
#    include <glu.h>        /* Header File For The GLU Library */
#  else
#    include <GL/gl.h>      /* Header File For The OpenGL Library */
#    include <GL/glu.h>     /* Header File For The GLU Library */
#  endif
#endif /* __EMSCRIPTEN__ */

#endif /* ENABLE_OPENGL */
