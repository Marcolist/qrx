#!/usr/bin/env bash
set -euo pipefail
BIN="${1:-./build/qrx}"; [[ -x "$BIN" ]]
T="$(mktemp -d /tmp/qrx7212-wal.XXXXXX)"; trap 'rm -rf "$T"' EXIT
export QRX_PASSPHRASE=testpass
"$BIN" seed-new "$T/w" >/dev/null; A="$($BIN address "$T/w"|tr -d '\r\n')"
"$BIN" init-chain "$T/base" 20 5000 2100000000000000 25000000 100000000000 qrx-regtest 1 5152583036 QRX-7212-WAL >/dev/null
"$BIN" faucet "$T/base" "$A" 30000000000 >/dev/null
"$BIN" create-velocity-raw-tx "$T/base" "$A" "$A" 12000000000 UNSIGNED UNSIGNED STAKE_BOND 1 1000 phase=7.2.12 1000 1 > "$T/raw"
"$BIN" signrawtransactionwithwallet "$T/w" "$T/base" "$T/raw" "$T/tx" >/dev/null
cp -a "$T/base" "$T/normal"; cp -a "$T/base" "$T/crash"
N="$($BIN applytx "$T/normal" "$T/tx" | sed -n 's/^state_root=//p')"
set +e; QRXDB_TEST_CRASH_AFTER_WAL_COMMIT=1 "$BIN" applytx "$T/crash" "$T/tx" >/dev/null 2>&1; RC=$?; set -e
[[ "$RC" -eq 86 ]]
C="$($BIN state-root "$T/crash" | sed -n 's/^state_root=//p')"; [[ "$C" == "$N" ]]
"$BIN" staking-status "$T/crash" "$A" | grep -q '^self_stake=12000000000$'
if "$BIN" verify "$T/crash" "$T/tx" >/dev/null 2>&1; then echo replay accepted after WAL recovery >&2; exit 1; fi
echo 'Phase 7.2.12 staking atomic WAL crash recovery PASS'
