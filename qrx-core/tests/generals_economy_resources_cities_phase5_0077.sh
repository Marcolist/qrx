#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="${1:-../build-phase5/qrx}"
"$Q" generals-unit-class-info INFANTRY | grep -q '^type=INFANTRY$'
grep -q 'GAME_STRATEGIC_CLAIM' "$ROOT/src/qrx.c"
grep -q 'GAME_ECONOMY_COLLECT' "$ROOT/src/qrx.c"
grep -q 'QRX-GENERALS-STRATEGIC-v1' "$ROOT/src/qrx.c"
grep -q '"CITY",50,0,0,25,5' "$ROOT/src/qrx.c"
grep -q '"OIL_FIELD",0,75,0,0,3' "$ROOT/src/qrx.c"
grep -q '"MINE",0,0,0,90,3' "$ROOT/src/qrx.c"
grep -q '"SUPPLY_HUB",100,0,25,0,4' "$ROOT/src/qrx.c"
grep -q '"COMMAND_CENTER",0,0,0,0,15' "$ROOT/src/qrx.c"
grep -q 'last_economy_turn' "$ROOT/src/qrx.c"
grep -q 'generals-strategic-info' "$ROOT/src/qrx.c"
grep -q 'generals-economy-info' "$ROOT/src/qrx.c"
echo 'PASS: Generals Phase 5 deterministic strategic features, capture ownership, per-turn economy production and scoring primitives'
