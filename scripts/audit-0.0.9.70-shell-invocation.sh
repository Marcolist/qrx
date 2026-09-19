#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$ROOT/scripts/build-all-targets.sh"
grep -Fq 'if bash "$ROOT/scripts/prepare-upscaler-ai-bundle.sh"' "$B"
test "$(grep -Fc 'bash "$ROOT/scripts/verify-upscaler-ai-bundle.sh"' "$B")" -ge 2
bash -n "$B"
echo "QRX 0.0.9.70 shell invocation audit: PASS"
