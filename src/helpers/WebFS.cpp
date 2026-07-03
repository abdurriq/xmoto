/*
 * WebFS.cpp — IDBFS helpers for the Emscripten/browser build (Phase 3).
 *
 * IDBFS is mounted from JavaScript (shell.html preRun) before main() runs,
 * so no EM_ASYNC_JS / Asyncify is needed at startup.  webfs_persist() uses
 * a fire-and-forget FS.syncfs(false, …) call at shutdown.
 */

#ifdef __EMSCRIPTEN__

#include "WebFS.h"
#include "helpers/Log.h"
#include <emscripten.h>

void webfs_mount_and_sync(const char *path) {
  /* IDBFS is already mounted by JavaScript preRun before main() starts.
     This stub just logs that the path is ready. */
  LogInfo("WebFS: user data ready at %s (IDBFS mounted by JS)", path);
}

void webfs_persist() {
  /* Fire-and-forget flush of the in-memory FS to IndexedDB.
     Called at orderly shutdown; errors are logged to the console. */
  EM_ASM(
    FS.syncfs(false, function(err) {
      if (err) console.warn('[xmoto] webfs_persist error:', err);
      else     console.log('[xmoto] User data persisted to IndexedDB.');
    });
  );
}

#endif /* __EMSCRIPTEN__ */

