#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
grep -q "addressDirty = false" "$ROOT/QRXBROWSER/src/app.js"
grep -q "ResizeObserver" "$ROOT/QRXBROWSER/src/app.js"
grep -q "browser_set_chrome_height" "$ROOT/QRXBROWSER/src-tauri/src/main.rs"
grep -q "validator-fleet-table-wrap" "$ROOT/GUIWALLET/src/index.html"
grep -q "QRX_BOOTSTRAP_VALIDATOR_COUNT 60" "$ROOT/qrx-core/src/genesis/qrx_bootstrap_validators.h"
grep -q "fill the 60 bootstrap validator addresses" "$ROOT/qrx-core/src/core_frontend.c"
[[ "$(grep -c 'qrx1bootstrap' "$ROOT/qrx-core/src/genesis/qrx_bootstrap_validators.c")" -eq 60 ]]
echo "QRX 0.0.9.68 browser/fleet rendering audit: PASS"
