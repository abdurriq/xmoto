#pragma once
/*
 * WebFS.h — IDBFS helpers for the Emscripten/browser build (Phase 3).
 *
 * Call webfs_mount_and_sync() once before XMFS::init() to mount the
 * persistent user-data directory backed by IndexedDB.  Call webfs_persist()
 * any time you want to flush in-memory changes back to IndexedDB (e.g. on
 * orderly shutdown or after saving the config).
 */

#ifdef __EMSCRIPTEN__
void webfs_mount_and_sync(const char *path);
void webfs_persist();
#endif
