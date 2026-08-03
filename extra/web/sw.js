/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Caches xmoto.js and xmoto.wasm for build-locked version consistency.
 * COOP/COEP headers are not injected here — GitHub Pages can't serve them
 * server-side, and SW-injected headers are unreliable across browsers.
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
  const isJs   = p.endsWith('.js') && p.indexOf('sw.js') === -1;
  const isWasm = p.endsWith('.wasm');
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
