/*
 * sw.js — X-Moto Service Worker
 *
 * Caches xmoto.wasm / xmoto.data / xmoto.js so that normal page refreshes
 * (including Firefox Ctrl+R) are served from the SW cache and never hit the
 * network for the large assets.
 *
 * Strategy: stale-while-revalidate
 *   – Return cached asset immediately (fast start)
 *   – Fetch fresh copy from server in the background and update cache
 *   – Next load picks up any new version automatically
 *
 * Hard refresh (Ctrl+Shift+R / Shift+F5) bypasses the SW entirely, so that
 * always picks up a freshly rebuilt version.
 */

const CACHE = 'xmoto-assets-v1';

function isCacheable(url) {
  const p = new URL(url).pathname;
  return p.endsWith('.wasm') || p.endsWith('.data') || p.endsWith('.js');
}

/* Install: take control immediately without waiting for old SW to die. */
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', evt =>
  evt.waitUntil(self.clients.claim())
);

self.addEventListener('fetch', evt => {
  if (!isCacheable(evt.request.url)) return; /* let browser handle normally */

  evt.respondWith(
    caches.open(CACHE).then(async cache => {
      const cached = await cache.match(evt.request);

      /* Always kick off a background network fetch to keep cache fresh.
         Do NOT await it here so we can return the cached response at once. */
      const update = fetch(evt.request.clone())
        .then(res => { if (res.ok) cache.put(evt.request, res.clone()); return res; })
        .catch(() => null);

      /* Serve cached immediately; if nothing cached yet, wait for network. */
      return cached ?? update;
    })
  );
});
