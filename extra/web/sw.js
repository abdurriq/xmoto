/*
 * sw.js — X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Purpose: override the `cache: 'no-cache'` mode that Chrome/Firefox inject
 * during Ctrl+R so that the HTTP cache (max-age=86400 + ETag) is consulted
 * instead of forcing a 115 MB re-download every reload.
 *
 * Strategy: pure network passthrough with `cache: 'default'`.
 *   – No assets are stored in the SW cache (eliminates version-mismatch bugs).
 *   – HTTP cache (served by the dev server) handles all caching.
 *   – Ctrl+Shift+R bypasses the SW entirely — use it to force a fresh build.
 */

function isCacheable(url) {
  const p = new URL(url).pathname;
  return (p.endsWith('.wasm') || p.endsWith('.data')) ||
         (p.endsWith('.js') && !p.endsWith('sw.js'));
}

self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', evt =>
  /* Purge any old SW caches from the previous stale-while-revalidate strategy */
  evt.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  )
);

self.addEventListener('fetch', evt => {
  if (!isCacheable(evt.request.url)) return; /* pass through unchanged */

  /* Re-issue the request with cache:'default' so the browser's HTTP cache
     (max-age + ETag) is used even when the page reload adds 'no-cache'.
     All three files (.wasm / .data / .js) then come from the same HTTP-cache
     generation, preventing offset mismatches that corrupt texture loading. */
  evt.respondWith(
    fetch(new Request(evt.request.url, {
      cache:       'default',
      credentials: evt.request.credentials,
    }))
  );
});


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
