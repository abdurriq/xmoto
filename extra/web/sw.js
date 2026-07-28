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
  const isNav  = req.mode === 'navigate';
  const isJs   = p.endsWith('.js')   && !p.endsWith('sw.js');
  const isWasm = p.endsWith('.wasm');
  const isData = p.endsWith('.data');

  // Navigation: fetch xmoto.html fresh and add COOP+COEP headers.
  // These two headers are all that is needed to set crossOriginIsolated=true
  // when COEP=credentialless is used (no CORP needed on same-origin assets).
  if (isNav) {
    evt.respondWith(
      fetch(req).then(function(res) {
        var h = new Headers(res.headers);
        h.set('Cross-Origin-Opener-Policy', 'same-origin');
        h.set('Cross-Origin-Embedder-Policy', 'credentialless');
        return new Response(res.body, {
          status: res.status, statusText: res.statusText, headers: h
        });
      })
    );
    return;
  }

  // Assets: serve from cache.  .js and .wasm also get CORP:cross-origin so
  // Firefox allows them to load inside pthread Web Workers (Firefox requires
  // this explicitly even for same-origin scripts under COEP=credentialless).
  // .data is served as-is (large file; workers don't load it directly).
  if (!isJs && !isWasm && !isData) return;

  evt.respondWith(
    caches.open(CACHE).then(function(cache) {
      return cache.match(req.url).then(function(cached) {
        var base = cached || null;
        var fromNet = !cached;
        return (fromNet ? fetch(req) : Promise.resolve(cached)).then(function(res) {
          if (fromNet && res.ok) cache.put(req.url, res.clone());
          // Add CORP header to JS and WASM so Firefox workers can load them.
          if ((isJs || isWasm) && res.ok) {
            var h = new Headers(res.headers);
            h.set('Cross-Origin-Resource-Policy', 'cross-origin');
            return new Response(res.body, {
              status: res.status, statusText: res.statusText, headers: h
            });
          }
          return res;
        });
      });
    })
  );
});
