#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
S="$R/scripts/prepare-upscaler-ai-bundle.sh"
bash -n "$S"
grep -q 'fetch_once()' "$S"
grep -q 'Downloaded bytes:' "$S"
grep -q 'Actual SHA-256:' "$S"
grep -q 'Expected SHA-256:' "$S"
grep -q 'Resolved URL:' "$S"
grep -q 'retained it in the QRX AI cache' "$S"
# No API digest discovery is permitted in the deterministic fetcher.
! grep -q 'api.github.com' "$S"
echo '0.0.9.64 deterministic AI fetch audit: PASS'
