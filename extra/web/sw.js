/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Caches xmoto.data and xmoto.js in the SW cache so that Firefox Ctrl+R
 * (which propagates cache:no-cache to all sub-resource fetches, bypassing
 * even explicit cache:'force-cache' in new Request() calls) cannot cause
 * a re-download.  The SW cache is completely immune to browser reload flags.
 *
 * On install: pre-cache xmoto.data + xmoto.js (uses HTTP cache if warm).
 * On activate: delete old caches from previous builds.
 * On fetch: serve .data/.js from SW cache; fall back to network if missing.
 *            .wasm passes through unmodified (streaming compile).
 *
 * Ctrl+Shift+R bypasses the SW entirely -> fresh files downloaded ->
 * HTTP cache populated -> next SW install picks them up via addAll().
 */

const CACHE    = 'xmoto-@BUILD_TS@';
const PRECACHE = ['xmoto.data', 'xmoto.js'];

self.addEventListener('install', evt =>
  evt.waitUntil(
    caches.open(CACHE)
      .then(cache => cache.addAll(PRECACHE))
      .then(() => self.skipWaiting())
  )
);

self.addEventListener('activate', evt =>
  evt.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  )
);

self.addEventListener('fetch', evt => {
  const p = new URL(evt.request.url).pathname;
  if (p.endsWith('.wasm')) return;
  const isJs = p.endsWith('.js') && !p.endsWith('sw.js');
  if (!p.endsWith('.data') && !isJs) return;

  evt.respondWith(
    caches.open(CACHE).then(cache =>
      cache.match(evt.request.url).then(cached => {
        if (cached) return cached;
        return fetch(evt.request).then(res => {
          if (res.ok) cache.put(evt.request.url, res.clone());
          return res;
        });
      })
    )
  );
});
