#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src/qrx.c"
GUI="$ROOT/../GUIWALLET/src/generals/index.html"
for tok in GAME_TECH_RECON GAME_RESEARCH_DISRUPT GAME_COUNTERINTEL_ACTIVATE RESEARCH_LAB UNIVERSITY generals-research-network-info generals-espionage-info generals_stage_tech_recon generals_stage_research_disrupt generals_stage_counterintel research_capacity disrupted_blocks; do
  grep -q "$tok" "$SRC" || { echo "missing $tok"; exit 1; }
done
for tok in 'digital-crown.mp3' 'grid-relay.mp3' 'gridfall-charge.mp3' 'grid-aurora-drift.mp3' 'toggleMusic' 'nextMusic'; do
  grep -q "$tok" "$GUI" || { echo "missing GUI/music $tok"; exit 1; }
done
for f in digital-crown.mp3 grid-relay.mp3 gridfall-charge.mp3 grid-aurora-drift.mp3; do
  test -s "$ROOT/../GUIWALLET/src/generals/audio/$f" || { echo "missing audio $f"; exit 1; }
done
echo 'Phase 5.3 structural/core/music checks: PASS'
