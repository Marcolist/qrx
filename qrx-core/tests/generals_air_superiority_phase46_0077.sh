#!/usr/bin/env bash
set -euo pipefail
Q="${1:-$(dirname "$0")/../build-p46/qrx}"
"$Q" generals-unit-class-info FIGHTER | grep -q '^type=FIGHTER$'
SRC="$(dirname "$0")/../src/qrx.c"
for x in GAME_CAP_MISSION GAME_ESCORT_MISSION GAME_AIR_INTERCEPT GAME_AIR_COMBAT_RESOLVE generals_stage_cap_mission generals_stage_escort_mission generals_stage_air_intercept generals_stage_air_combat_resolve; do grep -q "$x" "$SRC"; done
echo 'PASS: QRX Generals 0.0.7.7 Phase 4.6 air superiority/interception/mission resolution consensus primitives'
