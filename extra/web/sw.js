/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Two responsibilities:
 *
 * 1. Cross-Origin Isolation (COI): inject COOP + COEP=credentialless on the
 *    HTML navigation response so crossOriginIsolated=true in the page.
 *    With COEP=credentialless, same-origin assets need no extra headers —
 *    only the page itself needs COOP+COEP.  We do NOT modify asset responses
 *    to avoid breaking Content-Encoding (GitHub Pages serves assets gzip-
 *    compressed; copying headers into a new Response can cause browsers to
 *    try to decompress the already-compressed body a second time).
 *
 * 2. Build-locked caching: xmoto.data / xmoto.js / xmoto.wasm are served
 *    from cache so they always match the same build.  New build → new CACHE
 *    name → old cache deleted on activate.  Ctrl+Shift+R bypasses the SW.
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

  // Assets: serve from cache as-is.  Do NOT modify the response (would break
  // Content-Encoding handling for gzip-compressed assets from GitHub Pages).
  if (!isJs && !isWasm && !isData) return;

  evt.respondWith(
    caches.open(CACHE).then(function(cache) {
      return cache.match(req.url).then(function(cached) {
        if (cached) return cached;
        return fetch(req).then(function(res) {
          if (res.ok) cache.put(req.url, res.clone());
          return res;
        });
      });
    })
  );
});
