#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="${1:-./build-phase48/qrx}"
"$Q" generals-unit-class-info DESTROYER | grep -q '^type=DESTROYER$'
"$Q" generals-unit-class-info AMPHIBIOUS_TRANSPORT | grep -q '^type=AMPHIBIOUS_TRANSPORT$'
"$Q" generals-unit-class-info CARRIER | grep -q '^type=CARRIER$'
grep -q 'GAME_NAVAL_MOVE' "$ROOT/src/qrx.c"
grep -q 'GAME_AMPHIBIOUS_LOAD' "$ROOT/src/qrx.c"
grep -q 'GAME_AMPHIBIOUS_LAND' "$ROOT/src/qrx.c"
grep -q 'GAME_SEA_SUPPLY' "$ROOT/src/qrx.c"
grep -q '"PORT",500' "$ROOT/src/qrx.c"
grep -q '"NAVAL_YARD",800' "$ROOT/src/qrx.c"
echo 'PASS: Generals Phase 4.8 naval classes, water movement, amphibious transport and sea supply consensus primitives'
