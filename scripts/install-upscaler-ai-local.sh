#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 5 || $# -gt 6 ]]; then
  echo "usage: $0 <realesrgan-ncnn-vulkan> <model.param> <model.bin> <scale:2|4> <license-id> [model-id]" >&2
  exit 2
fi
runtime=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
param=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
weights=$(cd "$(dirname "$3")" && pwd)/$(basename "$3")
scale=$4
[[ "$scale" == 2 || "$scale" == 4 ]] || { echo "scale must be 2 or 4" >&2; exit 2; }
[[ -f "$runtime" && -f "$param" && -f "$weights" ]] || { echo "runtime/model file missing" >&2; exit 2; }
license_id=$5
[[ -n "$license_id" && "$license_id" != REVIEW-* && "$license_id" != UNKNOWN* ]] || { echo "A reviewed exact-artifact license/provenance id is required." >&2; exit 2; }
model_id=${6:-$(basename "$param" .param)}
case "$(uname -s)" in
  Darwin) base="$HOME/Library/Application Support/QRX/upscaler" ;;
  Linux) base="${XDG_DATA_HOME:-$HOME/.local/share}/qrx/upscaler" ;;
  *) echo "Use QRX_UPSCALER_NCNN_RUNTIME and QRX_UPSCALER_MODEL_DIR on this OS." >&2; exit 3 ;;
esac
rtdir="$base/runtime"; modeldir="$base/models"
mkdir -p "$rtdir" "$modeldir"
cp "$runtime" "$rtdir/realesrgan-ncnn-vulkan"
chmod 755 "$rtdir/realesrgan-ncnn-vulkan"
cp "$param" "$modeldir/$model_id.param"
cp "$weights" "$modeldir/$model_id.bin"
sha256(){ if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1"|awk '{print $1}'; else sha256sum "$1"|awk '{print $1}'; fi; }
ph=$(sha256 "$modeldir/$model_id.param")
wh=$(sha256 "$modeldir/$model_id.bin")
manifest="$modeldir/realesrgan-x${scale}plus.qrxmodel"
cat > "$manifest" <<MANIFEST
format=qrx-upscaler-model-v1
id=$model_id
scale=$scale
param=$model_id.param
weights=$model_id.bin
param_sha256=$ph
weights_sha256=$wh
license_id=$license_id
MANIFEST
echo "Installed runtime: $rtdir/realesrgan-ncnn-vulkan"
echo "Installed model:   $model_id (${scale}x)"
echo "Manifest:          $manifest"
echo "License/provenance id: $license_id"
