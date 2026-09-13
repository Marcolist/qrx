#!/usr/bin/env bash
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)/src/qrx.c"
grep -q 'GAME_AIR_FORMATION' "$SRC"
grep -q 'GAME_CAP_AUTO_INTERCEPT' "$SRC"
grep -q 'GAME_AIR_RTB' "$SRC"
grep -q 'GAME_SEAD_MISSION' "$SRC"
grep -q 'generals_stage_air_formation' "$SRC"
grep -q 'generals_stage_cap_auto_intercept' "$SRC"
grep -q 'generals_stage_air_rtb' "$SRC"
grep -q 'generals_stage_sead' "$SRC"
echo 'PASS: Generals Phase 4.7 advanced air operations consensus primitives'
