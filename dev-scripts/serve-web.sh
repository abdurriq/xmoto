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

LONG_CACHE  = 'public, max-age=86400'   # 24 h  — large static build artefacts
SHORT_CACHE = 'no-cache'                 # always revalidate — html / small files

class XMHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        ext = os.path.splitext(self.path)[1].lower()
        if ext in ('.wasm', '.data', '.js'):
            self.send_header('Cache-Control', LONG_CACHE)
        else:
            self.send_header('Cache-Control', SHORT_CACHE)
        super().end_headers()

    def log_message(self, fmt, *args):
        # Only log non-304 responses to keep output readable
        if args[1] != '304':
            super().log_message(fmt, *args)

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
with http.server.HTTPServer(('', port), XMHandler) as srv:
    srv.serve_forever()
PYEOF
