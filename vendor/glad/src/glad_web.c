/*
 * glad_web.c — no-op stub for Emscripten builds.
 *
 * All OpenGL ES 2 / WebGL2 symbols are provided directly by Emscripten's
 * GLES2 headers and the browser's WebGL2 implementation.  The glad loader
 * (which walks runtime function pointers for desktop GL) is not needed.
 */

int gladLoadGLLoader(void *p) { (void)p; return 1; }
int gladLoadGL(void)          { return 1; }
