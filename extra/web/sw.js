/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * 1. Cross-Origin Isolation: COOP+COEP on navigation responses (enables
 *    SharedArrayBuffer/pthreads in Chrome and Firefox; Safari ignores SW headers).
 * 2. Build-locked caching of .js/.wasm + CORP header for Firefox pthread workers.
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
