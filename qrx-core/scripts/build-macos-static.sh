#!/usr/bin/env bash
set -euo pipefail

ARCH="${1:-arm64}"
case "$ARCH" in
  arm64|aarch64) ARCH=arm64; OPENSSL_TARGET=darwin64-arm64-cc ;;
  x86_64|x64|amd64) ARCH=x86_64; OPENSSL_TARGET=darwin64-x86_64-cc ;;
  *) echo "Usage: $0 arm64|x86_64" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${QRX_OPENSSL_VERSION:-3.6.4}"
PREFIX="${PREFIX:-${QRX_OPENSSL_PREFIX:-$ROOT/../build/deps/openssl-macos-$ARCH}}"
BUILD_DIR="${BUILD_DIR:-$ROOT/../build/core/macos-${ARCH/arm64/arm64}}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
DEPS_ROOT="${QRX_DEPS_BUILD_ROOT:-$ROOT/../build/deps/src}"
TARBALL="$DEPS_ROOT/openssl-$VERSION.tar.gz"
SHA_FILE="$TARBALL.sha256"
SRC_DIR="$DEPS_ROOT/openssl-$VERSION-$ARCH"
BASE_URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION"

need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing build dependency: $1" >&2; exit 4; }; }
for c in curl perl make cmake clang ar ranlib shasum; do need "$c"; done
[[ "$(uname -s)" == Darwin ]] || { echo "This script requires macOS" >&2; exit 3; }
HOST_ARCH="$(uname -m)"
case "$ARCH:$HOST_ARCH" in
  arm64:arm64|x86_64:x86_64) ;;
  *) echo "Refusing non-native OpenSSL build: requested $ARCH, host is $HOST_ARCH" >&2; exit 3 ;;
esac

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
ACTUAL="$(shasum -a 256 "$TARBALL" | awk '{print $1}')"
ACTUAL_LC="$(printf '%s' "$ACTUAL" | tr '[:upper:]' '[:lower:]')"
EXPECTED_LC="$(printf '%s' "$EXPECTED" | tr '[:upper:]' '[:lower:]')"
[[ "$ACTUAL_LC" == "$EXPECTED_LC" ]] || { echo "OpenSSL SHA256 mismatch" >&2; echo "expected $EXPECTED" >&2; echo "actual   $ACTUAL" >&2; exit 5; }

echo "OpenSSL $VERSION source verified: $ACTUAL"

STAMP="$PREFIX/.qrx-openssl-$VERSION-$ARCH-static"
if [[ ! -f "$STAMP" || ! -f "$PREFIX/include/openssl/evp.h" || ! -f "$PREFIX/lib/libcrypto.a" ]]; then
  echo "Building static OpenSSL $VERSION for macOS $ARCH"
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

[[ -f "$PREFIX/lib/libcrypto.a" ]] || { echo "Static libcrypto.a missing after OpenSSL build" >&2; exit 6; }
if command -v lipo >/dev/null 2>&1; then
  LIB_ARCHS="$(lipo -archs "$PREFIX/lib/libcrypto.a" 2>/dev/null || true)"
  [[ -z "$LIB_ARCHS" || " $LIB_ARCHS " == *" $ARCH "* ]] || { echo "libcrypto.a has wrong architecture: $LIB_ARCHS" >&2; exit 6; }
fi

rm -rf "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
  -DQRX_REQUIRE_PQC=ON \
  -DOPENSSL_USE_STATIC_LIBS=TRUE \
  -DOPENSSL_ROOT_DIR="$PREFIX" \
  -DOPENSSL_INCLUDE_DIR="$PREFIX/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$PREFIX/lib/libcrypto.a"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

for exe in qrx qrx-cli qrxd qrxdb_verify qrxdb_salvage qrxdb_compact qrxdb_snapshot; do
  [[ -x "$BUILD_DIR/$exe" ]] || { echo "Missing QRX output: $BUILD_DIR/$exe" >&2; exit 7; }
  if command -v lipo >/dev/null 2>&1; then
    EXE_ARCHS="$(lipo -archs "$BUILD_DIR/$exe" 2>/dev/null || true)"
    [[ -z "$EXE_ARCHS" || " $EXE_ARCHS " == *" $ARCH "* ]] || { echo "$exe has wrong architecture: $EXE_ARCHS" >&2; exit 7; }
  fi
  if otool -L "$BUILD_DIR/$exe" 2>/dev/null | grep -Eiq 'libcrypto|libssl'; then
    echo "$exe unexpectedly depends on dynamic OpenSSL:" >&2
    otool -L "$BUILD_DIR/$exe" >&2
    exit 7
  fi
done

echo "QRX static macOS $ARCH build complete"
echo "OpenSSL prefix: $PREFIX"
echo "Core build dir: $BUILD_DIR"
