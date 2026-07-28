/*
 * sw.js - X-Moto Service Worker  (build @BUILD_TS@)
 *
 * Two responsibilities:
 *
 * 1. Cross-Origin Isolation (COI): inject COOP + COEP headers so that
 *    SharedArrayBuffer (required for WASM pthreads) is available.
 *    - Navigation (xmoto.html): COOP=same-origin + COEP=credentialless
 *    - All served resources: CORP=cross-origin
 *    We use COEP=credentialless (not require-corp) so that cross-origin
 *    XHR for level downloads still works without the remote server
 *    needing to send CORP headers.
 *
 * 2. Build-locked caching: xmoto.data / xmoto.js / xmoto.wasm are always
 *    served from the same build so they can never version-mismatch.
 *    Cache name includes a build timestamp; old caches are deleted on
 *    activate.  Ctrl+Shift+R bypasses the SW entirely.
 */

const CACHE = 'xmoto-@BUILD_TS@';

// Add cross-origin isolation headers to a Response.
// isNav=true  → also set COOP + COEP (needed on the HTML page itself)
// isNav=false → CORP only (allow the resource to be used cross-origin)
function coiWrap(response, isNav) {
  if (!response || response.status === 0) return response;
  const h = new Headers(response.headers);
  if (isNav) {
    h.set('Cross-Origin-Opener-Policy', 'same-origin');
    h.set('Cross-Origin-Embedder-Policy', 'credentialless');
  }
  h.set('Cross-Origin-Resource-Policy', 'cross-origin');
  return new Response(response.body, {
    status: response.status,
    statusText: response.statusText,
    headers: h,
  });
}

self.addEventListener('install', () => self.skipWaiting());

self.addEventListener('activate', evt =>
  evt.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  )
);

self.addEventListener('fetch', evt => {
  const req = evt.request;
  const p   = new URL(req.url).pathname;
  const isNav  = req.mode === 'navigate';
  const isJs   = p.endsWith('.js')   && !p.endsWith('sw.js');
  const isWasm = p.endsWith('.wasm');
  const isData = p.endsWith('.data');

  // Navigation: add COOP+COEP to the HTML page (no caching needed for HTML).
  if (isNav) {
    evt.respondWith(fetch(req).then(res => coiWrap(res, true)));
    return;
  }

  // Let all non-asset requests pass through untouched.
  if (!isJs && !isWasm && !isData) return;

  // Assets (.js/.wasm/.data): serve from cache with CORP header.
  evt.respondWith(
    caches.open(CACHE).then(cache =>
      cache.match(req.url).then(cached => {
        if (cached) return coiWrap(cached, false);
        return fetch(req).then(res => {
          if (res.ok) cache.put(req.url, res.clone());
          return coiWrap(res, false);
        });
      })
    )
  );
});
