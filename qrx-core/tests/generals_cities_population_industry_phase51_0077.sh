#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'GAME_CITY_DEVELOP' src/qrx.c
grep -q 'GAME_INDUSTRY_INVEST' src/qrx.c
grep -q 'generals-city-info' src/qrx.c
grep -q 'population' src/qrx.c
grep -q 'industrial_capacity' src/qrx.c
grep -q 'city_supply_bonus' src/qrx.c
grep -q 'industry_material_bonus' src/qrx.c
echo 'PASS: Generals Phase 5.1 city population, industry and economic development consensus structure'
