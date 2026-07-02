#!/usr/bin/env bash
# Serve the web build locally. WASM files must be served over HTTP (not file://)
# for IndexedDB and SharedArrayBuffer to work.
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
python3 -m http.server "$PORT"
