/*
 * WebFS.cpp — IDBFS helpers for the Emscripten/browser build (Phase 3).
 */

#ifdef __EMSCRIPTEN__

#include "WebFS.h"
#include "helpers/Log.h"
#include <emscripten.h>

/* Async-JS helper: wraps FS.syncfs() as an awaitable function via Asyncify.
   populate=1 → copy IndexedDB → memory FS (read saved data on startup)
   populate=0 → copy memory FS → IndexedDB (persist changes on save/exit)   */
EM_ASYNC_JS(void, webfs_idbfs_sync, (int populate), {
  await new Promise(function(resolve, reject) {
    FS.syncfs(populate ? true : false, function(err) {
      if (err) { console.error('IDBFS syncfs error:', err); }
      resolve();
    });
  });
});

void webfs_mount_and_sync(const char *path) {
  EM_ASM({
    var path = UTF8ToString($0);
    try { FS.mkdir(path); } catch(e) {}
    FS.mount(IDBFS, {}, path);
  }, path);

  /* Populate: copy existing IndexedDB data into the memory FS so that
     previously saved config/replays/DB are visible immediately.       */
  webfs_idbfs_sync(1);
  LogInfo("WebFS: mounted IDBFS at %s", path);
}

void webfs_persist() {
  webfs_idbfs_sync(0);
}

#endif /* __EMSCRIPTEN__ */
