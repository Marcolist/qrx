#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
grep -q 'fn markets_snapshot' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'fn markets_place_order' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'fn markets_cancel_order' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'data-view="markets"' "$ROOT/GUIWALLET/src/index.html"
grep -q 'drawMarketCandles' "$ROOT/GUIWALLET/src/index.html"
grep -q 'getorderbook' "$ROOT/qrx-core/src/qrxd.c"
grep -q 'listtrades' "$ROOT/qrx-core/src/qrxd.c"
grep -q 'createordertransaction' "$ROOT/qrx-core/src/qrxd.c"
grep -q 'createordercanceltransaction' "$ROOT/qrx-core/src/qrxd.c"
echo 'PASS: Phase 7 native markets/orderbook/trading view wiring'
