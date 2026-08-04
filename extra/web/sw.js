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
  const req  = evt.request;
  const p    = new URL(req.url).pathname;
  const isNav  = req.mode === 'navigate' || p.endsWith('.html');
  const isJs   = p.endsWith('.js') && p.indexOf('sw.js') === -1;
  const isWasm = p.endsWith('.wasm');

  // Inject COOP+COEP so crossOriginIsolated=true (Chrome/Firefox respect this;
  // Safari ignores SW-injected headers and the shell handles that gracefully).
  if (isNav) {
    evt.respondWith(
      fetch(req).then(function(res) {
        return res.text().then(function(body) {
          var h = new Headers(res.headers);
          h.set('Cross-Origin-Opener-Policy', 'same-origin');
          h.set('Cross-Origin-Embedder-Policy', 'credentialless');
          h.delete('Content-Encoding');
          h.delete('Content-Length');
          return new Response(body, { status: res.status, statusText: res.statusText, headers: h });
        });
      })
    );
    return;
  }

  if (!isJs && !isWasm) return;

  evt.respondWith(
    caches.open(CACHE).then(function(cache) {
      return cache.match(req.url).then(function(cached) {
        var fromNet = !cached;
        return (fromNet ? fetch(req) : Promise.resolve(cached)).then(function(res) {
          if (fromNet && res.ok) cache.put(req.url, res.clone());
          // CORP header lets Firefox pthread workers load xmoto.js under COEP.
          if (res.ok) {
            var h = new Headers(res.headers);
            h.set('Cross-Origin-Resource-Policy', 'cross-origin');
            return new Response(res.body, { status: res.status, statusText: res.statusText, headers: h });
          }
          return res;
        });
      });
    })
  );
});
