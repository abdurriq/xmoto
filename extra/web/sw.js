/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Caches xmoto.data, xmoto.js, and xmoto.wasm in the SW cache so that all
 * three always come from the same build and can never mismatch.  A mismatch
 * (e.g. old cached .js against a fresh .wasm) causes a LinkError on load.
 *
 * Files are cached lazily on first fetch; no eager install pre-caching that
 * would compete with the page load and abort wasm streaming compilation.
 * WebAssembly.instantiateStreaming() works with cached Response objects.
 *
 * Ctrl+Shift+R bypasses the SW entirely -> fresh files are fetched and
 * cached on the next normal page load.
 *
 * On new build (new timestamp): old cache deleted in activate, fresh files
 * cached on next request.
 */

const CACHE = 'xmoto-@BUILD_TS@';

self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', evt =>
  evt.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  )
);

self.addEventListener('fetch', evt => {
  const p = new URL(evt.request.url).pathname;
  const isJs   = p.endsWith('.js')   && !p.endsWith('sw.js');
  const isWasm = p.endsWith('.wasm');
  const isData = p.endsWith('.data');
  if (!isJs && !isWasm && !isData) return; /* let everything else through */

  evt.respondWith(
    caches.open(CACHE).then(cache =>
      cache.match(evt.request.url).then(cached => {
        if (cached) return cached;
        /* Not in SW cache yet: fetch fresh and cache for next reload */
        return fetch(evt.request).then(res => {
          if (res.ok) cache.put(evt.request.url, res.clone());
          return res;
        });
      })
    )
  );
});
