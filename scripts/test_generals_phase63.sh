#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
g="$root/GUIWALLET/src/generals/index.html"
w="$root/GUIWALLET/src/index.html"
r="$root/GUIWALLET/src-tauri/src/main.rs"
grep -q "GAME_NETWORK" "$g"
grep -q "demoSnapshot" "$g"
grep -q "SIMULATION.*no QUB spent" "$g"
! grep -q "generals_snapshot',{network:'alpha',wallet:'node1'" "$g"
grep -q 'open_generals_window",{network:appState.network,wallet:appState.wallet,demo:false}' "$w"
grep -q 'fn open_generals_window(app: tauri::AppHandle, network: Option<String>, wallet: Option<String>, demo: Option<bool>)' "$r"
echo "PASS: Generals Phase 6.3 sandbox/network static regression"
