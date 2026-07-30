/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Two responsibilities:
 *
 * 1. Cross-Origin Isolation: inject COOP + COEP=credentialless on the HTML
 *    navigation response so crossOriginIsolated=true (required for
 *    SharedArrayBuffer / pthreads).  Same-origin assets (.js, .wasm) also
 *    get CORP=cross-origin so Firefox allows them to load inside pthread
 *    Web Workers under COEP.
 *
 * 2. Build-locked caching: xmoto.data / xmoto.js / xmoto.wasm are always
 *    served from the same build so they can never version-mismatch.
 *    New build → new CACHE name → old cache deleted on activate.
 *    Ctrl+Shift+R bypasses the SW entirely.
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
  const isNav  = req.mode === 'navigate';
  const isJs   = p.endsWith('.js')   && !p.endsWith('sw.js');
  const isWasm = p.endsWith('.wasm');
  const isData = p.endsWith('.data');

  // Navigation: add COOP+COEP so crossOriginIsolated=true on the page.
  // Use res.text() so the body is always decoded text regardless of whether
  // the server sent Content-Encoding: gzip — avoids double-decompress in Safari.
  if (isNav) {
    evt.respondWith(
      fetch(req).then(function(res) {
        return res.text().then(function(body) {
          var h = new Headers(res.headers);
          h.set('Cross-Origin-Opener-Policy', 'same-origin');
          h.set('Cross-Origin-Embedder-Policy', 'credentialless');
          h.delete('Content-Encoding'); // body is decoded, no longer gzip
          h.delete('Content-Length');   // length changed after decoding
          return new Response(body, {
            status: res.status, statusText: res.statusText, headers: h
          });
        });
      })
    );
    return;
  }

  // Only cache .js and .wasm for version-locking; let .data bypass the SW.
  if (!isJs && !isWasm) return;

  evt.respondWith(
    caches.open(CACHE).then(function(cache) {
      return cache.match(req.url).then(function(cached) {
        var fromNet = !cached;
        return (fromNet ? fetch(req) : Promise.resolve(cached))
          .then(function(res) {
            if (fromNet && res.ok) cache.put(req.url, res.clone());
            if (res.ok) {
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
