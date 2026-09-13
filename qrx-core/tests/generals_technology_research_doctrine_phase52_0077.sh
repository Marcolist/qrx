#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=src/qrx.c
for tok in GAME_RESEARCH_START GAME_RESEARCH_ACCELERATE GAME_RESEARCH_COMPLETE GAME_DOCTRINE_SELECT generals-research-info generals-tech-tree generals-doctrine-info INDUSTRIAL_AUTOMATION_I LOGISTICS_NETWORK_I ADVANCED_ARMOR_I AIR_SUPERIORITY_II CARRIER_DOCTRINE_II ELECTRONIC_WARFARE_I; do
  grep -q "$tok" "$SRC"
done
grep -q 'generals_research_accel_blocks_per_qub",360' "$SRC"
grep -q 'cap=base/2' "$SRC"
grep -q 'generals:season:%lld:research_atoms' "$SRC"
grep -q 'generals:season:%lld:treasury_total_atoms' "$SRC"
grep -q 'No technology is QUB-exclusive' "$SRC"
BIN="${1:-build-phase52/qrx}"
"$BIN" generals-tech-tree /tmp/qrx-phase52-empty qrx1test 1 | grep -q 'INDUSTRIAL_AUTOMATION_I'
echo 'PHASE52_PASS'
