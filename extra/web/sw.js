/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Caches xmoto.data / xmoto.js / xmoto.wasm so they always come from the
 * same build (prevents .js/.wasm version mismatch / LinkError on reload).
 * Cache name includes a build timestamp; old caches are deleted on activate.
 * Ctrl+Shift+R bypasses the SW entirely.
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
  const p    = new URL(evt.request.url).pathname;
  const isJs   = p.endsWith('.js')   && !p.endsWith('sw.js');
  const isWasm = p.endsWith('.wasm');
  const isData = p.endsWith('.data');
  if (!isJs && !isWasm && !isData) return;

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
