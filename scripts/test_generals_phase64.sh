#!/usr/bin/env bash
set -euo pipefail
G=GUIWALLET/src/generals/index.html
T=GUIWALLET/src-tauri/src/main.rs
Q=qrx-core/src/qrx.c
D=qrx-core/src/qrxd.c
grep -q 'generals_prepare_offline_reveal' "$T"
grep -q 'OFFLINE COMMAND ARMED' "$G"
grep -q 'generals-offline-queue' "$Q"
grep -q 'generals-offline-process' "$Q"
grep -q 'generals-offline-process' "$D"
grep -q 'node_store_mempool_tx(chain_dir,tx)' "$Q"
echo 'PASS phase6.4 persistent world / offline command execution wiring'
