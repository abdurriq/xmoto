#!/usr/bin/env bash
# Serve the web build locally. WASM files must be served over HTTP (not file://)
# for IndexedDB and SharedArrayBuffer to work.
#
# Uses a tiny Python wrapper that adds Cache-Control headers so the browser
# caches xmoto.data / xmoto.wasm / xmoto.js for 24 hours.  After the first
# full load, page refreshes are near-instant.  Hard-refresh (Ctrl+Shift+R)
# bypasses the cache if you need to pick up a new build.
#
# Usage: bash dev-scripts/serve-web.sh [port]

PORT=${1:-8080}
BUILD_DIR="$(dirname "$0")/../build-web/src"

if [ ! -f "$BUILD_DIR/xmoto.html" ]; then
  echo "ERROR: build-web/src/xmoto.html not found."
  echo "Run the web build first:"
  echo "  source dev-scripts/env-web.sh"
  echo "  mkdir -p build-web && cd build-web"
  echo "  emcmake cmake .. -DEMSCRIPTEN_BUILD=ON -DCMAKE_BUILD_TYPE=Release"
  echo "  emmake make -j\$(nproc)"
  exit 1
fi

echo "Serving X-Moto web build at http://localhost:${PORT}/xmoto.html"
echo "Press Ctrl-C to stop."
cd "$BUILD_DIR"

python3 - "$PORT" <<'PYEOF'
import http.server, sys, os

def _cc(path):
    ext = os.path.splitext(path)[1].lower()
    base = os.path.basename(path)
    # sw.js must never be cached long-term so browsers always fetch the latest
    # Service Worker (which itself busts the asset cache on new builds).
    if base == 'sw.js':
        return 'no-cache'
    return 'public, max-age=86400' if ext in ('.wasm', '.data', '.js') else 'no-cache'

def _etag(path):
    try:
        s = os.stat(path)
        return f'"{int(s.st_mtime)}-{s.st_size}"'
    except OSError:
        return None

class XMHandler(http.server.SimpleHTTPRequestHandler):
    def send_head(self):
        path = self.translate_path(self.path)
        etag = _etag(path)
        # Return 304 if client already has this exact version (ETag match)
        if etag and self.headers.get('If-None-Match') == etag:
            self.send_response(304)
            self.send_header('ETag', etag)
            self.send_header('Cache-Control', _cc(path))
            http.server.BaseHTTPRequestHandler.end_headers(self)
            return None
        return super().send_head()

    def end_headers(self):
        path = self.translate_path(self.path)
        etag = _etag(path)
        if etag:
            self.send_header('ETag', etag)
        self.send_header('Cache-Control', _cc(path))
        super().end_headers()

    def log_message(self, fmt, *args):
        if args[1] != '304':
            super().log_message(fmt, *args)

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
with http.server.HTTPServer(('', port), XMHandler) as srv:
    srv.serve_forever()
PYEOF
