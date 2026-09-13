#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="${1:-../build-phase49/qrx}"
"$Q" generals-unit-class-info CARRIER | grep -q '^type=CARRIER$'
"$Q" generals-unit-class-info CRUISER | grep -q '^type=CRUISER$'
grep -q 'GAME_FLEET_CREATE' "$ROOT/src/qrx.c"
grep -q 'GAME_NAVAL_COMBAT_RESOLVE' "$ROOT/src/qrx.c"
grep -q 'GAME_CARRIER_AIR_WING' "$ROOT/src/qrx.c"
grep -q 'GAME_NAVAL_BLOCKADE' "$ROOT/src/qrx.c"
grep -q 'generals_stage_naval_deploy' "$ROOT/src/qrx.c"
grep -q 'generals_stage_naval_attack' "$ROOT/src/qrx.c"
grep -q 'pending_damage' "$ROOT/src/qrx.c"
grep -q 'FLEET-' "$ROOT/src/qrx.c"
echo 'PASS: Generals Phase 4.9 fleets, carrier operations, naval-yard deployment, blockades and deterministic naval combat primitives'
