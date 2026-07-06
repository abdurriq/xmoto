/*
 * sw.js — X-Moto Service Worker
 *
 * CACHE_NAME is stamped with the build timestamp at CMake configure time so
 * each new build gets a fresh cache.  The activate handler deletes all older
 * caches, forcing new assets to be downloaded exactly once per build.
 *
 * Ctrl+Shift+R (hard refresh) bypasses the SW entirely for explicit busts.
 */

const CACHE = 'xmoto-@BUILD_TS@';

function isCacheable(url) {
  const p = new URL(url).pathname;
  /* Cache the large wasm/data/js artefacts; NOT sw.js itself */
  return (p.endsWith('.wasm') || p.endsWith('.data')) ||
         (p.endsWith('.js') && !p.endsWith('sw.js'));
}

self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', evt =>
  evt.waitUntil(
    /* Purge caches from previous builds, then claim clients immediately */
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  )
);

self.addEventListener('fetch', evt => {
  if (!isCacheable(evt.request.url)) return;

  evt.respondWith(
    caches.open(CACHE).then(async cache => {
      const cached = await cache.match(evt.request);

      /* Always fetch fresh in background (stale-while-revalidate) */
      const update = fetch(evt.request.clone())
        .then(res => { if (res.ok) cache.put(evt.request, res.clone()); return res; })
        .catch(() => null);

      return cached ?? update;
    })
  );
});


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
