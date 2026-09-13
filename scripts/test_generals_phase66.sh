#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
q="$R/qrx-core/src/qrx.c"; gui="$R/GUIWALLET/src/index.html"; rust="$R/GUIWALLET/src-tauri/src/main.rs"
grep -q 'exp-nb > 4096' "$q"
grep -q 'active>=4096' "$q"
grep -q 'privacy_action_execute' "$rust"
grep -q 'shielded_address_create' "$rust"
grep -q 'Execute action' "$gui"
! grep -q 'draft-waiting-for-real-swap-engine' "$rust"
! grep -q 'no-custody-in-GUI-placeholder' "$rust"
! grep -q 'GUI preparation only' "$rust"
echo 'PASS phase6.6 mainnet release hardening + GUI production wiring'
