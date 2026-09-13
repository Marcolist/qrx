#!/usr/bin/env bash
set -euo pipefail
Q="${1:-$(dirname "$0")/../build-p45/qrx}"
"$Q" generals-unit-class-info FIGHTER | grep -q '^type=FIGHTER$'
"$Q" generals-unit-class-info BOMBER | grep -q '^type=BOMBER$'
"$Q" generals-unit-class-info SAM | grep -q '^type=SAM$'
grep -q 'GAME_AIR_MISSION' "$(dirname "$0")/../src/qrx.c"
grep -q 'GAME_RADAR_SCAN' "$(dirname "$0")/../src/qrx.c"
grep -q 'GAME_SAM_INTERCEPT' "$(dirname "$0")/../src/qrx.c"
grep -q 'GAME_MISSILE_LAUNCH' "$(dirname "$0")/../src/qrx.c"
grep -q 'GAME_STRIKE_RESOLVE' "$(dirname "$0")/../src/qrx.c"
grep -q 'RADAR_STATION' "$(dirname "$0")/../src/qrx.c"
grep -q 'SAM_SITE' "$(dirname "$0")/../src/qrx.c"
echo 'PASS: QRX Generals 0.0.7.7 Phase 4.5 air/radar/SAM/missile consensus primitives'
