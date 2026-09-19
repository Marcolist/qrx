#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
S="$R/scripts/prepare-upscaler-ai-bundle.sh"
B="$R/scripts/build-all-targets.sh"
grep -q 'upscaler-ai-archives.sha256' "$S"
grep -q 'QRX_AI_DOWNLOAD_CACHE' "$S"
grep -q -- '--retry-all-errors' "$S"
grep -q 'Refusing trust-on-first-use' "$S"
grep -q 'QRX_REQUIRE_VERIFIED_AI' "$B"
grep -q 'e5aa6eb131234b87c0c51f82b89390f5e3e642b7b70f2b9bbe95b6a285a40c96' "$R/scripts/upscaler-ai-archives.sha256"
echo '0.0.9.62 AI supply-chain/cache hardening audit: PASS'
