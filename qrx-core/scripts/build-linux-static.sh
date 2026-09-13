#!/usr/bin/env bash
set -euo pipefail

HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
  x86_64|amd64) ARCH=x86_64; OPENSSL_TARGET=linux-x86_64 ;;
  aarch64|arm64) ARCH=arm64; OPENSSL_TARGET=linux-aarch64 ;;
  *) echo "Unsupported native Linux architecture: $HOST_ARCH" >&2; exit 3 ;;
esac

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${QRX_OPENSSL_VERSION:-3.6.4}"
PREFIX="${PREFIX:-${QRX_OPENSSL_PREFIX:-$ROOT/../build/deps/openssl-linux-$ARCH}}"
BUILD_DIR="${BUILD_DIR:-$ROOT/../build/core/linux-$ARCH}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
DEPS_ROOT="${QRX_DEPS_BUILD_ROOT:-$ROOT/../build/deps/src}"
TARBALL="$DEPS_ROOT/openssl-$VERSION.tar.gz"
SHA_FILE="$TARBALL.sha256"
SRC_DIR="$DEPS_ROOT/openssl-$VERSION-linux-$ARCH"
BASE_URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION"

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing build dependency: $1" >&2; exit 4; }; }
for c in curl perl make cmake cc ar ranlib sha256sum; do need "$c"; done
[[ "$(uname -s)" == Linux ]] || { echo "This script requires Linux" >&2; exit 3; }
mkdir -p "$DEPS_ROOT" "$(dirname "$PREFIX")" "$(dirname "$BUILD_DIR")"

fetch() {
  local url="$1" out="$2"
  if [[ ! -s "$out" ]]; then
    echo "Downloading $(basename "$out")"
    curl --fail --location --retry 3 --retry-delay 2 --proto '=https' --tlsv1.2 -o "$out.tmp" "$url"
    mv "$out.tmp" "$out"
  fi
}
fetch "$BASE_URL/openssl-$VERSION.tar.gz" "$TARBALL"
fetch "$BASE_URL/openssl-$VERSION.tar.gz.sha256" "$SHA_FILE"
EXPECTED="$(awk 'NF {print $1; exit}' "$SHA_FILE")"
[[ "$EXPECTED" =~ ^[0-9a-fA-F]{64}$ ]] || { echo "Invalid OpenSSL SHA256 sidecar" >&2; exit 5; }
echo "$EXPECTED  $TARBALL" | sha256sum -c -

STAMP="$PREFIX/.qrx-openssl-$VERSION-linux-$ARCH-static"
if [[ ! -f "$STAMP" || ! -f "$PREFIX/include/openssl/evp.h" || ! -f "$PREFIX/lib/libcrypto.a" ]]; then
  echo "Building static OpenSSL $VERSION for Linux $ARCH"
  rm -rf "$SRC_DIR" "$PREFIX"
  mkdir -p "$SRC_DIR" "$PREFIX"
  tar -xzf "$TARBALL" --strip-components=1 -C "$SRC_DIR"
  (
    cd "$SRC_DIR"
    ./Configure "$OPENSSL_TARGET" no-shared no-tests --prefix="$PREFIX" --openssldir="$PREFIX/ssl"
    make -j"$JOBS"
    make install_sw
  )
  touch "$STAMP"
else
  echo "Reusing verified static OpenSSL $VERSION: $PREFIX"
fi

LIBCRYPTO="$PREFIX/lib/libcrypto.a"
[[ -f "$LIBCRYPTO" ]] || LIBCRYPTO="$PREFIX/lib64/libcrypto.a"
[[ -f "$LIBCRYPTO" ]] || { echo "Static libcrypto.a missing after OpenSSL build" >&2; exit 6; }
rm -rf "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DQRX_REQUIRE_PQC=ON \
  -DOPENSSL_USE_STATIC_LIBS=TRUE \
  -DOPENSSL_ROOT_DIR="$PREFIX" \
  -DOPENSSL_INCLUDE_DIR="$PREFIX/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$LIBCRYPTO"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

for exe in qrx qrx-cli qrxd qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot; do
  [[ -x "$BUILD_DIR/$exe" ]] || { echo "Missing QRX output: $BUILD_DIR/$exe" >&2; exit 7; }
  if ldd "$BUILD_DIR/$exe" 2>/dev/null | grep -Eiq 'libcrypto|libssl'; then
    echo "$exe unexpectedly depends on dynamic OpenSSL:" >&2
    ldd "$BUILD_DIR/$exe" >&2
    exit 7
  fi
done

echo "QRX static Linux $ARCH build complete"
echo "OpenSSL prefix: $PREFIX"
echo "Core build dir: $BUILD_DIR"
