#!/usr/bin/env bash
set -euo pipefail
HOST_ARCH="$(uname -m)"; case "$HOST_ARCH" in x86_64|amd64) ARCH=x86_64 ;; aarch64|arm64) ARCH=arm64 ;; *) echo "Unsupported Linux architecture: $HOST_ARCH" >&2; exit 3;; esac
[[ "$(uname -s)" == Linux ]] || { echo "This script requires Linux" >&2; exit 3; }
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"; BUILD_DIR="${BUILD_DIR:-$ROOT/../build/core/linux-$ARCH}"; DEPS_PREFIX="${QRX_NATIVE_DEPS_PREFIX:-$ROOT/../build/deps/linux-$ARCH}"
JOBS="$JOBS" QRX_NATIVE_DEPS_PREFIX="$DEPS_PREFIX" bash "$ROOT/scripts/build-unix-native-deps.sh" linux "$ARCH"
PNG_LIB="$DEPS_PREFIX/lib/libpng16.a"; [[ -f "$PNG_LIB" ]] || PNG_LIB="$DEPS_PREFIX/lib/libpng.a"
rm -rf "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DQRX_REQUIRE_PQC=ON -DQRX_REQUIRE_BUNDLED_DEPS=ON -DQRX_DEPS_PREFIX="$DEPS_PREFIX" \
  -DOPENSSL_USE_STATIC_LIBS=TRUE -DOPENSSL_ROOT_DIR="$DEPS_PREFIX" -DOPENSSL_INCLUDE_DIR="$DEPS_PREFIX/include" -DOPENSSL_CRYPTO_LIBRARY="$DEPS_PREFIX/lib/libcrypto.a" \
  -DZLIB_ROOT="$DEPS_PREFIX" -DZLIB_LIBRARY="$DEPS_PREFIX/lib/libz.a" -DZLIB_INCLUDE_DIR="$DEPS_PREFIX/include" \
  -DPNG_PNG_INCLUDE_DIR="$DEPS_PREFIX/include" -DPNG_LIBRARY="$PNG_LIB" \
  -DCURL_ROOT="$DEPS_PREFIX" -DCURL_USE_STATIC_LIBS=TRUE -DCURL_INCLUDE_DIR="$DEPS_PREFIX/include"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
for exe in qrx qrx-cli qrxd qrx-upscaler qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot; do
  [[ -x "$BUILD_DIR/$exe" ]] || { echo "Missing QRX output: $BUILD_DIR/$exe" >&2; exit 7; }
  deps="$(ldd "$BUILD_DIR/$exe" 2>/dev/null || true)"
  if printf '%s\n' "$deps" | grep -E 'libcrypto|libssl|libpng|libcurl|libz\.so' >/dev/null; then echo "$exe has forbidden external release dependency:" >&2; printf '%s\n' "$deps" >&2; exit 7; fi
done
echo "QRX hermetic Linux $ARCH build complete"
echo "Dependency prefix: $DEPS_PREFIX"
