#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./qrx}
"$Q" generals-unit-class-info RECON | grep -q '^vision='
"$Q" generals-unit-class-info MISSILE_BATTERY | grep -q '^max_range=256$'
grep -q 'GAME_RECON_SCAN' "$(dirname "$0")/../src/qrx.c"
grep -q 'GAME_EW_JAM' "$(dirname "$0")/../src/qrx.c"
grep -q 'generals_contact_visible(c,from,target,h)' "$(dirname "$0")/../src/qrx.c"
echo 'PASS: Generals Phase 4.4 static/compiled feature surface: Fog of War, Reconnaissance, EW, contact-gated long-range targeting'
