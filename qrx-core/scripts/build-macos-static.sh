#!/usr/bin/env bash
set -euo pipefail
ARCH="${1:-$(uname -m)}"
case "$ARCH" in arm64|aarch64) ARCH=arm64 ;; x86_64|amd64) ARCH=x86_64 ;; *) echo "Unsupported macOS architecture: $ARCH" >&2; exit 3;; esac
[[ "$(uname -s)" == Darwin ]] || { echo "This script requires macOS" >&2; exit 3; }
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
BUILD_DIR="${BUILD_DIR:-$ROOT/../build/core/macos-$ARCH}"
DEPS_PREFIX="${QRX_NATIVE_DEPS_PREFIX:-$ROOT/../build/deps/macos-$ARCH}"
JOBS="$JOBS" QRX_NATIVE_DEPS_PREFIX="$DEPS_PREFIX" bash "$ROOT/scripts/build-unix-native-deps.sh" macos "$ARCH"
PNG_LIB="$DEPS_PREFIX/lib/libpng16.a"; [[ -f "$PNG_LIB" ]] || PNG_LIB="$DEPS_PREFIX/lib/libpng.a"
rm -rf "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
  -DQRX_REQUIRE_PQC=ON -DQRX_REQUIRE_BUNDLED_DEPS=ON -DQRX_DEPS_PREFIX="$DEPS_PREFIX" \
  -DOPENSSL_USE_STATIC_LIBS=TRUE -DOPENSSL_ROOT_DIR="$DEPS_PREFIX" -DOPENSSL_INCLUDE_DIR="$DEPS_PREFIX/include" -DOPENSSL_CRYPTO_LIBRARY="$DEPS_PREFIX/lib/libcrypto.a" \
  -DZLIB_ROOT="$DEPS_PREFIX" -DZLIB_LIBRARY="$DEPS_PREFIX/lib/libz.a" -DZLIB_INCLUDE_DIR="$DEPS_PREFIX/include" \
  -DPNG_PNG_INCLUDE_DIR="$DEPS_PREFIX/include" -DPNG_LIBRARY="$PNG_LIB" \
  -DCURL_ROOT="$DEPS_PREFIX" -DCURL_USE_STATIC_LIBS=TRUE -DCURL_INCLUDE_DIR="$DEPS_PREFIX/include"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
for exe in qrx qrx-cli qrxd qrx-upscaler qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot; do
  [[ -x "$BUILD_DIR/$exe" ]] || { echo "Missing QRX output: $BUILD_DIR/$exe" >&2; exit 7; }
  deps="$(otool -L "$BUILD_DIR/$exe" 2>/dev/null || true)"
  if printf '%s\n' "$deps" | grep -E '/usr/local|/opt/homebrew|libcrypto|libssl|libpng|libcurl|libz\.' >/dev/null; then echo "$exe has forbidden external release dependency:" >&2; printf '%s\n' "$deps" >&2; exit 7; fi
done
echo "QRX hermetic macOS $ARCH build complete"
echo "Dependency prefix: $DEPS_PREFIX"
