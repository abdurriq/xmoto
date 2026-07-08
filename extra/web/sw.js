/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Prevents Ctrl+R from re-downloading the 115 MB .data file by overriding
 * cache:no-cache (injected by the browser on reload) with cache:default,
 * so the HTTP max-age/ETag cache is used instead.
 *
 * .wasm is NOT intercepted: WebAssembly.instantiateStreaming() requires the
 * browser to receive it directly. SW interception breaks streaming compile
 * and causes emscripten to fall back to ArrayBuffer (full re-download).
 *
 * Ctrl+Shift+R bypasses the SW entirely for a forced fresh download.
 */

self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', evt =>
  evt.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  )
);

self.addEventListener('fetch', evt => {
  const p = new URL(evt.request.url).pathname;
  if (p.endsWith('.wasm')) return;  /* pass .wasm through for streaming compile */
  const isJs = p.endsWith('.js') && !p.endsWith('sw.js');
  if (!p.endsWith('.data') && !isJs) return;
  evt.respondWith(
    fetch(new Request(evt.request.url, {
      /* force-cache: serve from HTTP cache regardless of the browser's
         reload flag.  Firefox Ctrl+R propagates cache:'no-cache' to
         all fetches; this override ensures the 115 MB .data file and
         .js are never re-downloaded on a soft refresh.
         Only Ctrl+Shift+R (hard refresh) bypasses the SW entirely and
         forces a fresh download from the server.                       */
      cache:       'force-cache',
      credentials: evt.request.credentials,
    }))
  );
});
