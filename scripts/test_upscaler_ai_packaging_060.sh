#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
S="$R/scripts/prepare-upscaler-ai-bundle.sh"
for t in macos-arm64 macos-x64 windows-x64 linux-x64 linux-arm64; do grep -q "$t" "$S"; done
grep -q 'realesrgan-ncnn-vulkan-20220424-windows.zip' "$S"
grep -q 'realesrgan-ncnn-vulkan-20220424-ubuntu.zip' "$S"
grep -q 'realesrgan-ncnn-vulkan-20220424-macos.zip' "$S"
grep -q 'RUNTIME_TAG="v0.2.0"' "$S"
grep -q 'RUNTIME_COMMIT_PREFIX="37026f4"' "$S"
grep -q 'Mach-O does not contain requested architecture' "$S"
grep -q 'PE is not x86-64' "$S"
grep -q 'ELF machine mismatch' "$S"
grep -q 'prepare-upscaler-ai-bundle.sh" "$TARGET"' "$R/scripts/build-all-targets.sh"
grep -q 'libvulkan-dev glslang-tools' "$R/.github/workflows/build-all-targets.yml"
echo '0.0.9.60 multi-platform AI packaging audit: PASS'
