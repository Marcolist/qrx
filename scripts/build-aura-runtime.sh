#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET=""; VARIANT=""; PLAN=0
LLAMA_REF="${QRX_LLAMA_CPP_REF:-b10878}"
while [[ $# -gt 0 ]]; do case "$1" in
  --target) TARGET="$2"; shift 2;;
  --variant) VARIANT="$2"; shift 2;;
  --plan) PLAN=1; shift;;
  -h|--help) echo "Usage: $0 --target <linux-x64|linux-arm64|macos-x64|macos-arm64|windows-x64> --variant <cpu|cuda|metal> [--plan]"; exit 0;;
  *) echo "Unknown option: $1" >&2; exit 2;; esac; done
[[ -n "$TARGET" && -n "$VARIANT" ]] || { echo "target and variant required" >&2; exit 2; }
row="$(awk -F'|' -v t="$TARGET" -v v="$VARIANT" '$1==t&&$2==v{print;exit}' "$ROOT/config/aura/runtime/runtime-build-matrix.qrx")"
[[ -n "$row" ]] || { echo "unsupported runtime matrix entry: $TARGET/$VARIANT" >&2; exit 3; }
IFS='|' read -r _ _ ADAPTER PLATFORM ARCH BACKEND FEATURES MINMEM CUMAJ CUMIN <<<"$row"
case "$VARIANT" in cpu) KIND=CPU;; cuda) KIND=CUDA;; metal) KIND=METAL;; *) exit 3;; esac
case "$TARGET" in
  linux-x64) EXT=.so;; linux-arm64) EXT=.so;; macos-x64) EXT=.dylib;; macos-arm64) EXT=.dylib;; windows-x64) EXT=.dll;; *) exit 3;; esac
case "$VARIANT" in cpu) BASE=qrx-aura-llama-cpu;; cuda) BASE=qrx-aura-llama-cuda;; metal) BASE=qrx-aura-llama-metal;; esac
OUT="$ROOT/dist/aura-runtimes/$TARGET-$VARIANT"; DEPS="$ROOT/build/aura-runtime-deps"; LLAMA="$DEPS/llama.cpp-$LLAMA_REF"; BUILD="$ROOT/build/aura-runtime/$TARGET-$VARIANT"
cat <<PLAN
QRX AURA runtime build
  target: $TARGET
  variant: $VARIANT
  adapter: $ADAPTER
  llama.cpp pin: $LLAMA_REF
  output: $OUT/$BASE$EXT
PLAN
[[ $PLAN -eq 1 ]] && exit 0
command -v git >/dev/null; command -v cmake >/dev/null; command -v python3 >/dev/null
mkdir -p "$DEPS" "$OUT" "$(dirname "$BUILD")"
if [[ ! -d "$LLAMA/.git" ]]; then git clone --filter=blob:none --depth 1 --branch "$LLAMA_REF" https://github.com/ggml-org/llama.cpp.git "$LLAMA"; fi
[[ "$(git -C "$LLAMA" describe --tags --exact-match 2>/dev/null || true)" == "$LLAMA_REF" ]] || { echo "llama.cpp checkout is not exact $LLAMA_REF" >&2; exit 4; }
rm -rf "$BUILD"
EXTRA=()
if [[ "$VARIANT" == cuda ]]; then EXTRA+=("-DCMAKE_CUDA_ARCHITECTURES=61;70;75;80;86"); fi
if [[ -n "${QRX_CMAKE_TOOLCHAIN_FILE:-}" ]]; then EXTRA+=("-DCMAKE_TOOLCHAIN_FILE=${QRX_CMAKE_TOOLCHAIN_FILE}"); fi
if [[ -n "${OPENSSL_ROOT_DIR:-}" ]]; then EXTRA+=("-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}"); fi
if [[ -n "${CURL_ROOT:-}" ]]; then EXTRA+=("-DCURL_ROOT=${CURL_ROOT}"); fi
cmake -S "$ROOT/qrx-core" -B "$BUILD" -DQRX_REQUIRE_PQC=OFF -DQRX_BUILD_TESTS=OFF -DQRX_BUILD_LLAMA_RUNTIME_PLUGIN=ON -DQRX_LLAMA_CPP_ROOT="$LLAMA" -DQRX_AURA_LLAMA_PLUGIN_KIND="$KIND" "${EXTRA[@]}"
cmake --build "$BUILD" --config Release --target qrx-aura-llama-runtime -j "${JOBS:-2}"
CANDIDATES=("$BUILD/$BASE$EXT" "$BUILD/Release/$BASE$EXT")
PLUGIN=""; for x in "${CANDIDATES[@]}"; do [[ -f "$x" ]] && PLUGIN="$x" && break; done
[[ -n "$PLUGIN" ]] || { echo "runtime plugin not found" >&2; find "$BUILD" -name "$BASE$EXT" -print; exit 5; }
cp "$PLUGIN" "$OUT/$BASE$EXT"
ROOTHEX="$(python3 - "$OUT/$BASE$EXT" <<'PY'
import hashlib,sys
h=hashlib.sha3_256()
with open(sys.argv[1],'rb') as f:
    for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
print(h.hexdigest())
PY
)"
FULLSHA="$(git -C "$LLAMA" rev-parse HEAD)"
ORIGIN_BASE="${QRX_RUNTIME_ORIGIN_BASE:-https://github.com/${GITHUB_REPOSITORY:-qrxchain/qrx-core}/releases/download/${GITHUB_REF_NAME:-v0.0.9-genesis}}"
cat > "$OUT/runtime-meta.env" <<META
publisher_id=qrx:runtime:official
package_id=qrx-aura-llama-$VARIANT-$TARGET
package_version=0.0.9-$LLAMA_REF
sequence=${QRX_RUNTIME_SEQUENCE:-1}
valid_from_height=${QRX_RUNTIME_VALID_FROM_HEIGHT:-0}
valid_until_height=${QRX_RUNTIME_VALID_UNTIL_HEIGHT:-18446744073709551615}
adapter=$ADAPTER
platform=$PLATFORM
arch=$ARCH
backend=$BACKEND
required_features=$FEATURES
min_memory_mib=$MINMEM
cuda_major=$CUMAJ
cuda_minor=$CUMIN
content_root=$ROOTHEX
download_uri=$ORIGIN_BASE/$BASE-$TARGET$EXT
install_filename=$BASE$EXT
artifact=$BASE$EXT
llama_ref=$LLAMA_REF
llama_commit=$FULLSHA
META
cp "$OUT/$BASE$EXT" "$OUT/$BASE-$TARGET$EXT"
echo "$ROOTHEX  $BASE-$TARGET$EXT" > "$OUT/$BASE-$TARGET$EXT.sha3-256"
echo "AURA runtime package ready: $OUT"
