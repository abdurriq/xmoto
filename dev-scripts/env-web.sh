#!/usr/bin/env bash
# Activate the pinned Emscripten SDK for X-Moto web builds.
# Run: source dev-scripts/env-web.sh

EMSDK_VERSION="3.1.50"
EMSDK_DIR="$HOME/emsdk"

if [ ! -f "$EMSDK_DIR/emsdk" ]; then
  echo "ERROR: emsdk not found at $EMSDK_DIR."
  echo "Run: git clone https://github.com/emscripten-core/emsdk.git ~/emsdk && cd ~/emsdk && ./emsdk install $EMSDK_VERSION && ./emsdk activate $EMSDK_VERSION"
  return 1
fi

source "$EMSDK_DIR/emsdk_env.sh" --quiet
echo "Emscripten $(emcc --version 2>&1 | head -1) active."

# Download libxml2 source if not already present (needed for emscripten build)
LIBXML2_DIR="$(dirname "$0")/../vendor/libxml2"
if [ ! -f "$LIBXML2_DIR/configure.ac" ]; then
  echo "Downloading libxml2 2.9.14 into vendor/libxml2 ..."
  mkdir -p "$LIBXML2_DIR"
  curl -sL "https://download.gnome.org/sources/libxml2/2.9/libxml2-2.9.14.tar.xz" \
    | tar -xJ -C "$LIBXML2_DIR" --strip-components=1
  echo "libxml2 ready."
fi
