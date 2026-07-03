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

# Build xmoto.bin (packed game assets) from a native build if not present.
# The web build preloads this file via --preload-file.
BIN_FILE="$(dirname "$0")/../bin/xmoto.bin"
if [ ! -f "$BIN_FILE" ]; then
  echo "xmoto.bin not found — building native xmoto to create it..."
  NATIVE_DIR="$(dirname "$0")/../build-native"
  mkdir -p "$NATIVE_DIR"
  (cd "$NATIVE_DIR" && cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_GETTEXT=OFF \
      -DCMAKE_EXE_LINKER_FLAGS="-lGLU" -DEMSCRIPTEN_BUILD=OFF > /dev/null 2>&1)
  make -C "$NATIVE_DIR" -j"$(nproc)" xmoto 2>&1 | tail -3
  "$NATIVE_DIR/src/xmoto" --pack "$BIN_FILE" "$(dirname "$0")/../bin" 2>&1 | tail -2
  echo "xmoto.bin ready ($(du -sh "$BIN_FILE" | cut -f1))."
fi
