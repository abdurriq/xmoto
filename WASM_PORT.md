# X-Moto WASM/Emscripten Port Plan

This document is both the specification and the step-by-step execution checklist for
porting X-Moto to the browser via Emscripten. Each phase ends with a concrete
verification gate that must pass before the next phase begins.

---

## Source-code snapshot assumptions

All paths are relative to the repository root. The analysis below reflects the state
of the `master` branch as of 2026-07-02.

---

## Overall scope for the first browser release (MVP)

**In:**
- Single-player gameplay on bundled levels
- Keyboard and gamepad input
- Audio (SDL_mixer)
- Persistent player profile + config (IndexedDB-backed IDBFS)
- Local replays (stored in IDBFS)
- Software-rendered menus and HUD using the existing OpenGL draw layer

**Out of scope for MVP (to be re-enabled in later phases):**
- Multiplayer (`NetClient`/`NetServer`/`SDL_net`)
- Web highscore downloads and uploads (libcurl / `src/common/WWW.cpp`)
- Background level/theme update threads
- Standalone server mode (`--server-only`)
- Video recording (`VideoRecorder`)
- `--pack`/`--unpack` command-line tools

---

## Dependency matrix

| Library | Browser strategy |
|---|---|
| SDL2 | Emscripten built-in port (`-s USE_SDL=2`) |
| SDL2_mixer | Emscripten port (`-s USE_SDL_MIXER=2`) |
| SDL2_ttf | Emscripten port (`-s USE_SDL_TTF=2`) |
| SDL2_net | **Disabled** for MVP (`-DENABLE_NETWORK=OFF`) |
| libcurl | **Disabled** for MVP (`-DENABLE_WWW=OFF`) |
| libxml2 | Compile from vendored or system source via `emcmake` |
| zlib | Emscripten port (`-s USE_ZLIB=1`) |
| libpng | Emscripten port (`-s USE_LIBPNG=1`) |
| libjpeg | Vendor and compile with `emcmake` |
| SQLite | Compile from source with `emcc` (pure C, no OS deps) |
| bzip2 | Vendored in `vendor/bzip2`, compile with `emcc` |
| Lua | Vendored in `vendor/lua`, compile with `emcc` |
| ODE | Vendored in `vendor/ode`, compile with `emcc` |
| chipmunk | Vendored in `vendor/chipmunk`, compile with `emcc` |
| libccd | Vendored in `vendor/libccd`, compile with `emcc` |
| md5sum | Vendored in `vendor/md5sum`, compile with `emcc` |
| xdgbasedir | **Disabled** for web (`-DUSE_XDG=OFF`); paths hardcoded |
| glad | **Replaced** — Emscripten provides WebGL2 natively; glad stub needed |
| gettext/intl | Disabled for MVP (`-DUSE_GETTEXT=OFF`) |
| OpenGL | **Migrated** from legacy fixed-function to GLES2 / WebGL2 |

---

## Phase 0 — Environment setup

### 0.1  Install the Emscripten SDK

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk
./emsdk install 3.1.50   # pin a specific version for reproducibility
./emsdk activate 3.1.50
source ./emsdk_env.sh
```

Add `source ~/emsdk/emsdk_env.sh` to the project's `dev-scripts/env-web.sh`
so every developer activates the same toolchain.

### 0.2  Verify basic toolchain

```bash
emcc --version          # must print the pinned version
emcmake cmake --version # must find CMake ≥ 3.12
```

### 0.3  Create `CMake/Emscripten.cmake`

This toolchain file is the CMake entry-point for web builds:

```cmake
# CMake/Emscripten.cmake
set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_C_COMPILER   emcc)
set(CMAKE_CXX_COMPILER em++)
set(CMAKE_AR           emar)
set(CMAKE_RANLIB       emranlib)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

**Checkpoint 0** — `emcc --version` prints cleanly; `dev-scripts/env-web.sh`
sources the SDK without error.

---

## Phase 1 — New CMake build profile (`EMSCRIPTEN_BUILD`)

### 1.1  Add top-level option

In `CMakeLists.txt`, after the existing `project()` line:

```cmake
option(EMSCRIPTEN_BUILD "Build for the browser via Emscripten" OFF)
```

All subsequent phases gate their logic on `EMSCRIPTEN_BUILD`.

### 1.2  Add feature-disable options wired to `EMSCRIPTEN_BUILD`

In `src/CMakeLists.txt`, add:

```cmake
option(ENABLE_NETWORK   "Enable multiplayer networking (SDL_net)"      ON)
option(ENABLE_WWW       "Enable web highscore/download features (curl)" ON)
option(ENABLE_THREADING "Enable background worker threads"              ON)
option(ENABLE_SERVER    "Enable standalone server mode"                 ON)

if(EMSCRIPTEN_BUILD)
  set(ENABLE_NETWORK   OFF CACHE BOOL "" FORCE)
  set(ENABLE_WWW       OFF CACHE BOOL "" FORCE)
  set(ENABLE_THREADING OFF CACHE BOOL "" FORCE)
  set(ENABLE_SERVER    OFF CACHE BOOL "" FORCE)
  set(USE_GETTEXT      OFF CACHE BOOL "" FORCE)
  set(PREFER_SYSTEM_XDG OFF CACHE BOOL "" FORCE)
  set(USE_OPENGL        ON  CACHE BOOL "" FORCE)
endif()
```

### 1.3  Guard SDL_net detection

Wrap the `find_package(SDL2_net REQUIRED)` block:

```cmake
if(ENABLE_NETWORK)
  find_package(SDL2_net REQUIRED)
endif()
```

Do the same for `find_package(CURL REQUIRED)`:

```cmake
if(ENABLE_WWW)
  find_package(CURL REQUIRED)
endif()
```

### 1.4  Guard SDL_net / curl link libraries

In the `target_link_libraries(xmoto ...)` block, wrap each entry:

```cmake
$<$<BOOL:${ENABLE_NETWORK}>:${SDL2_NET_LIBRARY}>
$<$<BOOL:${ENABLE_WWW}>:${CURL_LIBRARIES}>
```

### 1.5  Add Emscripten link flags

At the end of `src/CMakeLists.txt`, add a dedicated block:

```cmake
if(EMSCRIPTEN_BUILD)
  set_target_properties(xmoto PROPERTIES SUFFIX ".html")

  target_compile_options(xmoto PRIVATE
    -s USE_SDL=2
    -s USE_SDL_MIXER=2
    -s USE_SDL_TTF=2
    -s USE_ZLIB=1
    -s USE_LIBPNG=1
  )

  set(EM_LINK_FLAGS
    "-s USE_SDL=2"
    "-s USE_SDL_MIXER=2"
    "-s USE_SDL_TTF=2"
    "-s USE_ZLIB=1"
    "-s USE_LIBPNG=1"
    "-s MIN_WEBGL_VERSION=2"
    "-s MAX_WEBGL_VERSION=2"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=134217728"   # 128 MB
    "-s EXPORTED_RUNTIME_METHODS=['callMain','FS','PATH']"
    "-s ASYNCIFY=1"                 # needed during VFS sync; see Phase 3
    "--preload-file ${CMAKE_SOURCE_DIR}/bin@/"
    "-lidbfs.js"
  )
  string(JOIN " " EM_LINK_FLAGS_STR ${EM_LINK_FLAGS})
  set_target_properties(xmoto PROPERTIES LINK_FLAGS "${EM_LINK_FLAGS_STR}")
endif()
```

### 1.6  Replace glad with a web no-op

`glad` loads desktop OpenGL function pointers at runtime; Emscripten bakes WebGL2
directly into the `GLES2/gl2.h` header. Create `vendor/glad/src/glad_web.c`:

```c
/* glad_web.c — no-op stub for Emscripten builds */
/* All GL symbols are provided by Emscripten's GLES2 headers. */
int gladLoadGLLoader(void *p) { (void)p; return 1; }
int gladLoadGL(void)          { return 1; }
```

In `vendor/glad/CMakeLists.txt`, select the source:

```cmake
if(EMSCRIPTEN_BUILD)
  add_library(glad STATIC src/glad_web.c)
  target_include_directories(glad PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
else()
  add_library(glad STATIC src/glad.c)
  target_include_directories(glad PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
endif()
```

In `src/include/xm_OpenGL.h`, add the Emscripten include path:

```c
#ifdef ENABLE_OPENGL
#ifdef __EMSCRIPTEN__
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>
#else
#  include <glad.h>
/* ... existing desktop includes ... */
#endif
#endif
```

### 1.7  Build smoke test

```bash
source ~/emsdk/emsdk_env.sh
mkdir -p build-web && cd build-web
emcmake cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../CMake/Emscripten.cmake \
  -DEMSCRIPTEN_BUILD=ON \
  -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc) 2>&1 | tee build.log
```

**Checkpoint 1** — `build.log` shows no missing-library errors for the disabled
subsystems. The build may still fail on undefined symbols from networking/curl/thread
code — those are resolved in the next phases. The compiler does not reject any
vendored C/C++ source file outright.

---

## Phase 2 — Stub disabled subsystems

All stubs use `#ifdef __EMSCRIPTEN__` guards so the native build is untouched.

### 2.1  Network stubs (`ENABLE_NETWORK=OFF`)

Add to `src/net/NetClient.cpp` and `src/net/NetServer.cpp` at the top, after
existing includes:

```cpp
#ifndef ENABLE_NETWORK

// --- Web stubs ---
#include "NetClient.h"
#include "NetServer.h"

void NetClient::connect(const std::string &, int) {}
void NetClient::disconnect() {}
bool NetClient::isConnected() { return false; }
void NetClient::manageNetwork(int, xmDatabase *) {}
// ... (stub every remaining public method to no-op or throw a disabled error)

void NetServer::start(bool, int, const std::string &) {}
void NetServer::stop() {}
bool NetServer::isStarted() { return false; }

#endif // !ENABLE_NETWORK
```

Surround the entire existing implementation in each file:
```cpp
#ifdef ENABLE_NETWORK
// ... existing code ...
#endif
```

### 2.2  WWW/curl stubs (`ENABLE_WWW=OFF`)

In `src/common/WWW.cpp`, wrap the entire file body:

```cpp
#ifdef ENABLE_WWW
// ... existing curl code ...
#else
// Minimal stubs so callers compile:
#include "WWW.h"
// Return "offline" for every method; throw Exception("Web features disabled").
#endif
```

In `src/xmoto/GameInit.cpp`, where `curl_global_init` is called, add:

```cpp
#ifdef ENABLE_WWW
  curl_global_init(CURL_GLOBAL_ALL);
#endif
```

### 2.3  Threading stubs (`ENABLE_THREADING=OFF`)

`XMThread::startThread()` in `src/thread/XMThread.cpp`:

```cpp
void XMThread::startThread() {
#ifdef ENABLE_THREADING
  m_progress = -1;
  m_currentOperation = "";
  m_askThreadToEnd = false;
  m_isRunning = true;
  m_pThread = SDL_CreateThread(&XMThread::run, NULL, this);
#else
  // Run synchronously on the caller's stack (progress states will still work).
  runInMain();
#endif
}
```

For states that spin up long-running background threads (download, sync,
check-www) and display a progress UI, gate them behind:

```cpp
#ifdef ENABLE_THREADING
  m_checkWwwThread->startThread();
#else
  // Skip the www check entirely on web.
#endif
```

Affected call sites:
- `src/states/StateMainMenu.cpp` lines ~215, ~1986, ~2055, ~2063
- `src/thread/XMThreadStats.cpp` line ~153
- `src/thread/DownloadReplaysThread.cpp` line ~113
- `src/states/StateMultiUpdate.cpp` line ~87
- `src/states/StateUpdate.cpp` line ~105
- `src/net/NetServer.cpp` line ~41

### 2.4  Server-only mode stubs

Wrap the `sigaction` block in `src/xmoto/GameInit.cpp`:

```cpp
#if !defined(WIN32) && defined(ENABLE_SERVER)
  if (v_xmArgs.isOptServerOnly()) {
    // ... sigaction setup ...
  }
#endif
```

In `GameApp::initNetwork`, wrap the server-start block:
```cpp
#ifdef ENABLE_SERVER
  if (i_forceNoServerStarted == false) { ... }
#endif
```

### 2.5  Video recorder stub

`VideoRecorder` uses raw file I/O outside the virtual FS. Gate at usage sites:

```cpp
#ifndef __EMSCRIPTEN__
  if (v_xmArgs.isOptVideoRecording()) { /* ... */ }
#endif
```

### 2.6  XDG / environment path stubs

`XMFS::init` calls `xdgInitHandle` on non-Windows. For Emscripten, we hardcode
paths (see Phase 4). For now, add a compile guard in `src/common/VFileIO.cpp`:

```cpp
#ifndef WIN32
# ifndef __EMSCRIPTEN__
  // xdg
  if ((m_xdgHd = (xdgHandle *)malloc(sizeof(xdgHandle))) == NULL) {
    throw Exception("xdgbasedir allocation failed");
  }
  if ((xdgInitHandle(m_xdgHd)) == 0) {
    throw Exception("xdgbasedir initialisation failed");
  }
# endif
#endif
```

**Checkpoint 2** — Running `emmake make` produces a `.wasm` artifact with no
undefined-symbol linker errors. The build may still crash at runtime — that is
resolved in later phases.

---

## Phase 3 — Filesystem and asset packaging

### 3.1  Define virtual FS paths

In `src/common/VFileIO.cpp`, add an Emscripten branch inside `XMFS::init`:

```cpp
#ifdef __EMSCRIPTEN__
  // All game data is mounted at /xmoto-data by the preload step.
  // User data lives in /xmoto-user, which is backed by IDBFS.
  m_SystemDataDir  = "/xmoto-data";
  m_UserDataDir    = "/xmoto-user/data";
  m_UserConfigDir  = "/xmoto-user/config";
  m_UserCacheDir   = "/xmoto-user/cache";
  m_UserDataDirUTF8 = m_UserDataDir;
  m_bGotSystemDataDir = true;
  m_SystemLocaleDir = ""; // gettext disabled for MVP
  // mkArborescenceDir calls follow normally — they work on the virtual FS.
#else
  // ... existing platform blocks ...
#endif
```

### 3.2  Mount IDBFS at startup

Create `src/helpers/WebFS.h` and `src/helpers/WebFS.cpp`:

```cpp
// WebFS.h
#pragma once
#ifdef __EMSCRIPTEN__
void webfs_mount_and_sync(const char *path, int populate);
void webfs_persist();
#endif

// WebFS.cpp
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void webfs_mount_and_sync(const char *path, int populate) {
  EM_ASM({
    FS.mkdir(UTF8ToString($0));
    FS.mount(IDBFS, {}, UTF8ToString($0));
  }, path);
  // Synchronous populate (blocking via Asyncify) on first run:
  emscripten_idbfs_sync(populate); // populate=1 on first launch
}

void webfs_persist() {
  EM_ASM({ FS.syncfs(false, function(err) {}); });
}
#endif
```

Add `WebFS.cpp` to the `helpers_src` list in `src/CMakeLists.txt`.

### 3.3  Call the mount early in `GameApp::run_load`

In `src/xmoto/GameInit.cpp`, before `XMFS::init(...)`:

```cpp
#ifdef __EMSCRIPTEN__
  webfs_mount_and_sync("/xmoto-user", 1);
#endif
  XMFS::init("xmoto", "xmoto.bin", ...);
```

After `run_unload` saves configuration:

```cpp
#ifdef __EMSCRIPTEN__
  webfs_persist();
#endif
```

### 3.4  Asset preloading

The `--preload-file` flag added in Phase 1.5 copies the `bin/` tree into the
Emscripten virtual FS at path `/`. This makes `xmoto.bin`, `Levels/`, `Textures/`,
`Themes/`, `Shaders/`, and `Physics/` available without extra work.

Verify the mapping is correct: `XMFS::init` tries to open `xmoto.bin` from the
current directory first, then `bin/xmoto.bin`, then the system data dir. In the
web build, the current directory IS `/` in the VFS, so `xmoto.bin` will be found
at `/xmoto.bin` from the preloaded `bin/` tree.

### 3.5  Remove `getPWDDir` call on web

`src/common/VFileIO.cpp` line ~117 calls `getenv("PWD")`. Guard it:

```cpp
std::string getPWDDir(bool i_asUtf8) {
#ifdef __EMSCRIPTEN__
  return "/";
#elif defined(WIN32)
  return win32_getPWDDir(i_asUtf8);
#else
  return std::string(getenv("PWD"));
#endif
}
```

### 3.6  SQLite build

SQLite is a dependency via `find_package(SQLITE3 REQUIRED)`. For the web build,
vendor SQLite into the repository under `vendor/sqlite3/` or point to a local
copy, then compile it with `emcc`. The easiest path is to use Emscripten's
SQLite port via `-s USE_SQLITE=1` if available, otherwise compile with emcc.

Add to `src/CMakeLists.txt` under the Emscripten block:

```cmake
if(EMSCRIPTEN_BUILD)
  # Use emscripten-compiled sqlite
  add_library(sqlite3 STATIC ${CMAKE_SOURCE_DIR}/vendor/sqlite3/sqlite3.c)
  target_include_directories(sqlite3 PUBLIC ${CMAKE_SOURCE_DIR}/vendor/sqlite3)
  set(SQLITE3_LIBRARIES sqlite3)
  set(SQLITE3_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/vendor/sqlite3)
endif()
```

**Checkpoint 3** — The browser console shows no "file not found" errors for
`xmoto.bin`. The game progresses past `XMFS::init`. User-data directories are
created under `/xmoto-user/`. A browser restart (same origin) finds the saved
config.

---

## Phase 4 — Main loop adaptation

### 4.1  Understand the current structure

`GameApp::run` calls `run_load()`, then `run_loop()`, then `run_unload()`.
`run_loop()` is a blocking `while (m_bQuit == false)` loop
(`src/xmoto/GameInit.cpp` line 891). This cannot run in a browser.

### 4.2  Extract the per-frame function

In `src/xmoto/Game.h`, add:

```cpp
  void run_one_frame();  // executes exactly one frame worth of work
```

In `src/xmoto/GameInit.cpp`, refactor `run_loop`:

```cpp
void GameApp::run_loop() {
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(
    [](void *arg) {
      GameApp *app = static_cast<GameApp *>(arg);
      app->run_one_frame();
      if (app->m_bQuit) {
        emscripten_cancel_main_loop();
        app->run_unload();
      }
    },
    this,
    0,    // let the browser control FPS via requestAnimationFrame
    1     // simulate_infinite_loop = 1 keeps the C stack clean
  );
#else
  while (m_bQuit == false) {
    run_one_frame();
  }
#endif
}

void GameApp::run_one_frame() {
  // Move the entire body of the original while-loop here verbatim.
  SDL_Event Event;
  // ... (existing loop body) ...
}
```

### 4.3  Remove `SDL_Delay` in the web path

`GameApp::wait()` calls `SDL_Delay(delta)`. In the browser, `requestAnimationFrame`
already throttles rendering; blocking with `SDL_Delay` will freeze the tab.

```cpp
void GameApp::wait(int &io_lastFrameTimeStamp, int &io_frameLate, int i_maxFps) {
#ifndef __EMSCRIPTEN__
  // ... existing SDL_Delay logic ...
#else
  // The browser RAF callback handles timing.
  io_lastFrameTimeStamp = getXMTimeInt();
#endif
}
```

### 4.4  Adjust `SDL_WaitEvent` usage

The main loop calls `SDL_WaitEvent` when no update/render is needed (iconified
state). This blocks indefinitely in the browser. Replace with `SDL_PollEvent`:

```cpp
  if (StateManager::instance()->needUpdateOrRender()) {
    while (SDL_PollEvent(&Event)) { manageEvent(&Event); }
  } else {
#ifndef __EMSCRIPTEN__
    if (SDL_WaitEvent(&Event) == 1) { manageEvent(&Event); }
    while (SDL_PollEvent(&Event)) { manageEvent(&Event); }
#else
    // Never block in the browser — just poll.
    while (SDL_PollEvent(&Event)) { manageEvent(&Event); }
#endif
  }
```

**Checkpoint 4** — The browser tab loads without freezing. The main menu renders
(even if visually incorrect at this stage). The browser DevTools console shows
no "unresponsive script" warning. Opening and closing the tab does not leave a
spinning loop.

---

## Phase 5 — OpenGL → GLES2 / WebGL2 migration

This is the largest phase. The renderer uses:

1. **Fixed-function immediate mode** (`glBegin`/`glEnd`/`glVertex*`/`glTexCoord*`)
   — not available in WebGL. Every call site must be rewritten.
2. **Fixed-function matrix stack** (`glMatrixMode`/`glLoadIdentity`/`glOrtho`/
   `glPushMatrix`/`glPopMatrix`/`glRotatef`/`glScalef`/`glTranslatef`) —
   not available in WebGL. Must be replaced with manually maintained matrices
   passed as shader uniforms.
3. **Legacy client-side arrays** (`glEnableClientState`/`glVertexPointer`/
   `glTexCoordPointer`/`glColorPointer`) — replaced by VAOs/VBOs.
4. **Legacy extension ARB shader API** (`glCreateShaderObjectARB` etc. in
   `RendererFBO.cpp`) — replaced by core GLES2 shader API.
5. **`glLineWidth`** — supported in WebGL2 only for `1.0`; wider lines require
   geometry shaders or CPU tessellation.

### 5.1  Introduce a thin 2D rendering abstraction

Rather than converting every raw GL call, introduce a minimal 2D drawing
back-end that the existing `DrawLib` virtual interface delegates to. The goal
is to isolate WebGL2 divergences in one place.

Create `src/drawlib/DrawLibGLES2.h` and `src/drawlib/DrawLibGLES2.cpp`.
`DrawLibGLES2` extends `DrawLib` identically to `DrawLibOpenGL` but uses
GLES2-compatible code paths.

In `src/drawlib/DrawLib.cpp`, add:

```cpp
#ifdef __EMSCRIPTEN__
#  include "DrawLibGLES2.h"
#endif
```

And in `DrawLib::DrawLibFromName`:

```cpp
#ifdef __EMSCRIPTEN__
  m_backend = backend_OpenGl;
  return new DrawLibGLES2();
#endif
```

The following sub-tasks all live inside `DrawLibGLES2.cpp`.

### 5.2  Core shader pair

Write one vertex shader and one fragment shader that handle all 2D use cases
(textured quads, solid-color polygons, lines):

```glsl
// src/drawlib/gles2_2d.vert
#version 100
attribute vec2 a_pos;
attribute vec2 a_uv;
attribute vec4 a_color;
uniform mat4 u_mvp;
varying vec2 v_uv;
varying vec4 v_color;
void main() {
  v_uv    = a_uv;
  v_color = a_color;
  gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
}

// src/drawlib/gles2_2d.frag
#version 100
precision mediump float;
uniform sampler2D u_tex;
uniform int u_useTexture;
varying vec2 v_uv;
varying vec4 v_color;
void main() {
  if (u_useTexture == 1)
    gl_FragColor = texture2D(u_tex, v_uv) * v_color;
  else
    gl_FragColor = v_color;
}
```

Compile these at `DrawLibGLES2::init` time. Cache the uniform and attribute
locations.

### 5.3  Replace `glBegin`/`glEnd` immediate-mode batches

All `DrawLib::startDraw()` / `endDraw()` calls buffer geometry into a CPU-side
`std::vector<float>`. On `endDraw()`, flush via a single `glDrawArrays` using
a temporary VBO.

Add to `DrawLib`:

```cpp
struct Vertex2D { float x, y, u, v; uint8_t r, g, b, a; };
std::vector<Vertex2D> m_drawBatch;
DrawMode              m_currentDrawMode;
```

`startDraw(mode)` clears the batch and records the mode.
`glVertexSP(x,y)` / `glVertex(x,y)` append to the batch using the current colour.
`glTexCoord(u,v)` records UV for the next vertex.
`endDraw()` calls `flushBatch()` which uploads and draws the batch.

### 5.4  Replace the matrix stack

Add to `DrawLibGLES2`:

```cpp
#include "helpers/VMath.h"  // uses existing Vector/Matrix utilities
struct MatrixStack {
  std::vector<Matrix44> stack;
  void push();
  void pop();
  void loadIdentity();
  void ortho(float l, float r, float b, float t, float n, float f);
  void translate(float x, float y, float z);
  void rotate(float angle, float x, float y, float z);
  void scale(float x, float y, float z);
  const Matrix44 &top() const;
};
MatrixStack m_proj, m_model;
```

Replace every `glMatrixMode(GL_PROJECTION)` call with a switch of the active
stack pointer. Compute `u_mvp = proj.top() * model.top()` and upload before
each draw call.

### 5.5  Replace legacy client-state arrays in `Renderer.cpp`

The renderer has two VBO render paths already:

- Lines ~2076–2135 and ~2279–2399 use `glEnableClientState` + `glVertexPointer`
  + `glTexCoordPointer`.
- The non-VBO path uses immediate mode.

For the GLES2 back-end, always use the VBO path but through the GLES2-compatible
generic attrib API:

```cpp
glBindBuffer(GL_ARRAY_BUFFER, vboId);
glBufferData(GL_ARRAY_BUFFER, dataSize, dataPtr, GL_STATIC_DRAW);
glEnableVertexAttribArray(a_pos_loc);
glVertexAttribPointer(a_pos_loc, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
glEnableVertexAttribArray(a_uv_loc);
glVertexAttribPointer(a_uv_loc, 2, GL_FLOAT, GL_FALSE, stride, (void*)8);
```

### 5.6  Fix `SFXOverlay` (RendererFBO) shaders

`SFXOverlay.vert` / `SFXOverlay.frag` use `gl_TexCoord`, `ftransform()`, and
`gl_MultiTexCoord0` — all fixed-function GLSL 1.10 built-ins not in GLES2.

Create versioned alternates `SFXOverlay.vert.es` and `SFXOverlay.frag.es` using
ESSL 1.00 with explicit attributes / uniforms. Load them when `__EMSCRIPTEN__`
is defined:

```cpp
const std::string suffix = /* EMSCRIPTEN */ ".es" : "";
_SetShaderSource(m_VertShaderID, "SFXOverlay.vert" + suffix);
_SetShaderSource(m_FragShaderID, "SFXOverlay.frag" + suffix);
```

Replace the `glCreateShaderObjectARB` / `glUseProgramObjectARB` ARB extension
calls with the core GLES2 equivalents (`glCreateShader` / `glUseProgram` etc.)
under an `#ifdef __EMSCRIPTEN__` branch.

### 5.7  Fix `SDL_GL_SetAttribute` for GLES2 context

In `DrawLibGLES2::init` (or in `DrawLibOpenGL::init` under a guard), request a
GLES2 context before window creation:

```cpp
#ifdef __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
```

### 5.8  `glLineWidth` fallback

`DrawLibOpenGL::setLineWidth` calls `glLineWidth(width)`. WebGL2 only guarantees
a width of 1.0. For MVP, clamp to 1.0 on web:

```cpp
void DrawLibGLES2::setLineWidth(float width) {
  (void)width;
  glLineWidth(1.0f);  // WebGL2 only supports 1.0
}
```

Wider lines (debug overlays) can be replaced with a thin quad geometry later.

**Checkpoint 5** — The main menu renders correctly in the browser with no WebGL
errors in the console (`gl.getError()` returns 0 at end of each frame). Level
gameplay is visually correct: terrain, bike, HUD, and ghost trail all draw. The
FBO-based ghost blur effect either renders or is gracefully disabled.

---

## Phase 6 — Audio

### 6.1  Browser audio context unlock

Browsers require a user gesture before audio can start. SDL2's Emscripten port
handles this automatically for `Mix_PlayChannel` calls — the first sound is
deferred until the first pointer or keyboard event. No special code is required.

Verify by checking `SDL_GetError()` after `Mix_OpenAudio`; it should be empty.

### 6.2  Audio file loading from VFS

`Sound::loadSample` opens files via `XMFS::openIFile`, which reads from the
virtual FS preloaded from `bin/`. No change required; the existing `SDL_RWops`
bridge works with the emscripten VFS.

### 6.3  OGG codec

Confirm that the `SDL2_mixer` Emscripten port is compiled with OGG/Vorbis
support (`-s SDL2_MIXER_FORMATS=["ogg"]` in the link flags if needed).

**Checkpoint 6** — Menu music plays after the first mouse click in the browser.
Sound effects trigger during gameplay. No audio errors appear in the console.

---

## Phase 7 — Input and browser UX

### 7.1  Keyboard input

SDL2's Emscripten backend maps browser `KeyboardEvent` to `SDL_Keycode`
automatically. Verify the default X-Moto bindings (arrow keys, Space, Escape)
work out of the box.

### 7.2  Canvas sizing

Add to the Emscripten link flags:

```
"-s CANVAS_ELEMENT_ID='canvas'"
```

In the HTML shell (see §7.4), the canvas is sized to the viewport and the
default resolution (800×600 or full-viewport) is set via a `Module.canvas`
override.

### 7.3  Pointer lock and fullscreen

The browser requires a user gesture for fullscreen. Disable automatic fullscreen
on web by treating `m_bWindowed = true` unconditionally when
`__EMSCRIPTEN__` is defined in `DrawLibGLES2::init`.

### 7.4  Custom HTML shell

Create `extra/web/shell.html` — a minimal HTML page that:
- Embeds the canvas
- Shows a "Click to start" overlay (removes after first interaction to unlock
  audio)
- Resizes the canvas on `window.resize`
- Links the generated `.js` file

```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>X-Moto</title>
  <style>
    body { margin:0; background:#000; display:flex;
           align-items:center; justify-content:center; height:100vh; }
    #canvas { display:block; }
    #overlay { position:absolute; color:#fff; font-size:2em;
               cursor:pointer; user-select:none; }
  </style>
</head>
<body>
  <div id="overlay">Click to start</div>
  <canvas id="canvas"></canvas>
  <script>
    var Module = {
      canvas: document.getElementById('canvas'),
      onRuntimeInitialized: function() {
        document.getElementById('overlay').style.display = 'none';
      }
    };
  </script>
  <script src="xmoto.js"></script>
</body>
</html>
```

Pass to the link step: `--shell-file ${CMAKE_SOURCE_DIR}/extra/web/shell.html`.

**Checkpoint 7** — The game launches with a click, the canvas fills the
viewport, arrow keys drive the bike, the Escape menu works, and audio plays.
No JS exceptions appear in the console.

---

## Phase 8 — Persistent saves verification

### 8.1  Config persistence

1. Start the game, go to Options, change a setting (e.g. audio volume).
2. Close the browser tab.
3. Reopen; verify the setting persists.
4. Check IndexedDB in DevTools > Application > IndexedDB; the `/xmoto-user` FS
   entries must be present.

### 8.2  Replay saving

1. Complete a level.
2. Choose to save the replay.
3. Check `DevTools > Application > IndexedDB` for a file under
   `/xmoto-user/data/Replays/`.
4. Reload the game and verify the replay appears in the replays list.

### 8.3  Level progress (database)

1. Complete a level.
2. Reload.
3. Open the level in the list; verify the best-time badge appears, confirming
   SQLite data survived the page reload via IDBFS.

**Checkpoint 8** — All three save scenarios pass. The `webfs_persist()` call
in `run_unload` is exercised (observable via a console log added temporarily).

---

## Phase 9 — CI / CD integration

### 9.1  GitHub Actions web build job

Create `.github/workflows/build-web.yml`:

```yaml
name: Web build

on: [push, pull_request]

jobs:
  build-web:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Set up Emscripten
        uses: mymindstorm/setup-emsdk@v14
        with:
          version: 3.1.50

      - name: Configure
        run: |
          mkdir build-web && cd build-web
          emcmake cmake .. \
            -DCMAKE_TOOLCHAIN_FILE=CMake/Emscripten.cmake \
            -DEMSCRIPTEN_BUILD=ON \
            -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: emmake make -j4 -C build-web

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: xmoto-web
          path: build-web/src/xmoto.*
```

### 9.2  GitHub Pages deployment (optional)

Add a second job to `build-web.yml` that deploys to `gh-pages` branch on
push to `master`, using `actions/deploy-pages`.

### 9.3  Local test server

Add `dev-scripts/serve-web.sh`:

```bash
#!/usr/bin/env bash
# COOP/COEP headers are only needed if pthread support is added later.
cd build-web/src
python3 -m http.server 8080
```

WASM files must be served over HTTP (not file://) for IndexedDB to work.

**Checkpoint 9** — The CI job builds green on the `master` branch. The artifact
download produces an `xmoto.html` + `xmoto.js` + `xmoto.wasm` triplet that can
be extracted and played locally via the serve script.

---

## Phase 10 — Performance and size tuning (post-MVP)

These tasks are not required for the first release but must be tracked.

| Task | Why |
|---|---|
| Enable LTO (`-flto`) | Reduces `.wasm` size by ~20–30% |
| Compress `.wasm` with Brotli/Gzip | Server-side; reduces first load time |
| Profile with `--profiling` emcc flag + Chrome DevTools | Identify JS<>WASM overhead |
| Replace CPU geometry batching with persistent VBOs | Reduces per-frame upload cost |
| Upgrade GLES2 shaders to GLES3 (WebGL2 core) | Enables instancing, UBOs |
| Implement geometry-based thick lines | Restores debug overlays |

---

## Phase 11 — Post-MVP feature restoration

These features are explicitly deferred and each requires its own mini-plan.

| Feature | Blocker | Approach |
|---|---|---|
| Web highscores / level downloads | libcurl not available | Replace `FSWeb` methods with `fetch()` via `emscripten_fetch` |
| Multiplayer | SDL_net not available | Redesign transport as a WebSocket abstraction; write a `NetTransport` interface |
| Background download threads | Browser threading constraints | Use `emscripten_fetch` async API; remove thread dependency entirely |
| Gettext / i18n | Disabled in MVP | Re-enable with pre-compiled `.mo` files preloaded into VFS |

---

## Appendix A — Build command reference

```bash
# One-time SDK activation
source ~/emsdk/emsdk_env.sh

# Configure
mkdir -p build-web && cd build-web
emcmake cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../CMake/Emscripten.cmake \
  -DEMSCRIPTEN_BUILD=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_GETTEXT=OFF

# Build
emmake make -j$(nproc)

# Serve locally (required for IndexedDB)
cd src && python3 -m http.server 8080
# Open http://localhost:8080/xmoto.html
```

---

## Appendix B — Checkpoint summary

| # | Gate condition |
|---|---|
| 0 | `emcc --version` prints cleanly; env script sources without error |
| 1 | `emmake make` produces no missing-library linker errors for disabled subsystems |
| 2 | `emmake make` produces a `.wasm` artifact with zero undefined-symbol linker errors |
| 3 | Browser console shows no "file not found" for `xmoto.bin`; VFS mounts; config survives reload |
| 4 | Browser tab loads without freeze; main menu renders without console errors |
| 5 | WebGL2 console shows zero `getError()` hits; menu and gameplay are visually correct |
| 6 | Menu music and game SFX play after first user interaction |
| 7 | Arrow keys drive the bike; Escape opens the pause menu; canvas sizes correctly |
| 8 | Config, replays, and level times all survive page reloads via IndexedDB |
| 9 | CI build job is green; artifact plays from local HTTP server |

---

## Appendix C — Files changed / created per phase

| Phase | Files modified | Files created |
|---|---|---|
| 0 | — | `CMake/Emscripten.cmake`, `dev-scripts/env-web.sh` |
| 1 | `CMakeLists.txt`, `src/CMakeLists.txt`, `src/include/xm_OpenGL.h`, `vendor/glad/CMakeLists.txt` | `vendor/glad/src/glad_web.c` |
| 2 | `src/net/NetClient.cpp`, `src/net/NetServer.cpp`, `src/common/WWW.cpp`, `src/thread/XMThread.cpp`, `src/states/StateMainMenu.cpp`, `src/states/StateUpdate.cpp`, `src/states/StateMultiUpdate.cpp`, `src/thread/XMThreadStats.cpp`, `src/thread/DownloadReplaysThread.cpp`, `src/xmoto/GameInit.cpp`, `src/common/VFileIO.cpp` | — |
| 3 | `src/common/VFileIO.cpp`, `src/xmoto/GameInit.cpp`, `src/CMakeLists.txt` | `src/helpers/WebFS.h`, `src/helpers/WebFS.cpp` |
| 4 | `src/xmoto/GameInit.cpp`, `src/xmoto/Game.h` | — |
| 5 | `src/drawlib/DrawLib.cpp`, `src/xmoto/RendererFBO.cpp`, `src/xmoto/Renderer.cpp` | `src/drawlib/DrawLibGLES2.h`, `src/drawlib/DrawLibGLES2.cpp`, `src/drawlib/gles2_2d.vert`, `src/drawlib/gles2_2d.frag`, `bin/Shaders/SFXOverlay.vert.es`, `bin/Shaders/SFXOverlay.frag.es` |
| 6 | `src/CMakeLists.txt` (SDL_mixer codec flags) | — |
| 7 | `src/drawlib/DrawLibGLES2.cpp`, `src/CMakeLists.txt` | `extra/web/shell.html` |
| 8 | `src/xmoto/GameInit.cpp` | — |
| 9 | — | `.github/workflows/build-web.yml`, `dev-scripts/serve-web.sh` |
