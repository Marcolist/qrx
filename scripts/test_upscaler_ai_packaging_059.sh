#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
bash -n "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'realesrgan-ncnn-vulkan-20220424-macos.zip' "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'sha256:'  "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'Mach-O does not contain requested architecture' "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'realesr-animevideov3-x2' "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'realesrgan-x4plus' "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'qrx-upscaler-model-v2' "$R/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'runtime_verified' "$R/qrx-core/src/apps/qrx_upscaler_ai.c"
grep -q 'resources/upscaler/\*\*/\*' "$R/GUIWALLET/src-tauri/tauri.conf.json"
grep -q 'QRX_UPSCALER_RUNTIME_SHA256' "$R/GUIWALLET/src-tauri/src/main.rs"
echo '0.0.9.59 AI packaging audit: PASS'
