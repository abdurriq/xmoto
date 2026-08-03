/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * 1. Injects COOP+COEP on the navigation response so crossOriginIsolated=true
 *    in Chrome and Firefox (pthreads/SharedArrayBuffer).  Safari ignores
 *    SW-injected headers; the shell handles that gracefully.
 * 2. Caches xmoto.js and xmoto.wasm for build-locked version consistency.
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
    // res.text() transparently decompresses gzip; we drop Content-Encoding so
    // the browser doesn't try to decompress the already-decoded string again.
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
      return cache.match(evt.request.url).then(function(cached) {
        if (cached) return cached;
        return fetch(evt.request).then(function(res) {
          if (res.ok) cache.put(evt.request.url, res.clone());
          return res;
        });
      });
    })
  );
});
