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
import http.server, sys, os, urllib.request, urllib.parse

def _cc(path):
    ext = os.path.splitext(path)[1].lower()
    base = os.path.basename(path)
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
    def do_GET(self):
        # CORS proxy: /xm-proxy/?url=<encoded>  — forwards request server-side
        if self.path.startswith('/xm-proxy/'):
            qs = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(qs)
            url = params.get('url', [''])[0]
            if not url:
                self.send_error(400, 'Missing url parameter')
                return
            try:
                req = urllib.request.Request(url, headers={
                    'User-Agent': 'xmoto-web/1.0'})
                with urllib.request.urlopen(req, timeout=30) as resp:
                    data = resp.read()
                self.send_response(200)
                self.send_header('Content-Type',
                                 resp.headers.get('Content-Type',
                                                  'application/octet-stream'))
                self.send_header('Content-Length', str(len(data)))
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(data)
            except Exception as e:
                self.send_error(502, f'Proxy error: {e}')
            return

        path = self.translate_path(self.path)
        etag = _etag(path)
        if etag and self.headers.get('If-None-Match') == etag:
            self.send_response(304)
            self.send_header('ETag', etag)
            self.send_header('Cache-Control', _cc(path))
            http.server.BaseHTTPRequestHandler.end_headers(self)
            return
        super().do_GET()

    def send_head(self):
        path = self.translate_path(self.path)
        etag = _etag(path)
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
