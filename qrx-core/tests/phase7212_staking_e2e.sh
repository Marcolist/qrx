#!/usr/bin/env bash
set -euo pipefail
BIN="${1:-./build/qrx}"
[[ -x "$BIN" ]] || { echo "missing qrx: $BIN" >&2; exit 1; }
T="$(mktemp -d /tmp/qrx7212-staking.XXXXXX)"; trap 'rm -rf "$T"' EXIT
export QRX_PASSPHRASE=testpass
"$BIN" seed-new "$T/alice" >/dev/null
"$BIN" seed-new "$T/validator" >/dev/null
A="$($BIN address "$T/alice"|tr -d '\r\n')"; V="$($BIN address "$T/validator"|tr -d '\r\n')"
"$BIN" init-chain "$T/chain" 20 5000 2100000000000000 25000000 100000000000 qrx-regtest 1 5152583036 QRX-7212-E2E >/dev/null
# Test-only zero-height unbonding; chain.meta is the active regtest parameter file.
printf 'staking_unbonding_blocks=0\ndelegation_unbonding_blocks=0\n' >> "$T/chain/chain.meta"
"$BIN" faucet "$T/chain" "$A" 50000000000 >/dev/null
"$BIN" faucet "$T/chain" "$V" 50000000000 >/dev/null
make_tx(){ local W="$1" FROM="$2" TO="$3" AMT="$4" TYPE="$5" NONCE="$6" OUT="$7"; 
  "$BIN" create-velocity-raw-tx "$T/chain" "$FROM" "$TO" "$AMT" UNSIGNED UNSIGNED "$TYPE" 1 1000 phase=7.2.12 1000 "$NONCE" > "$OUT.raw"
  "$BIN" signrawtransactionwithwallet "$W" "$T/chain" "$OUT.raw" "$OUT.signed" >/dev/null
  "$BIN" verify "$T/chain" "$OUT.signed" >/dev/null
}
# validator self-bond 20B
make_tx "$T/validator" "$V" "$V" 20000000000 STAKE_BOND 1 "$T/s1"
cp "$T/s1.signed" "$T/malformed.signed"; sed -i.bak 's/^amount=20000000000$/amount=20000000001/' "$T/malformed.signed" || true
if "$BIN" verify "$T/chain" "$T/malformed.signed" >/dev/null 2>&1; then echo malformed signature accepted >&2; exit 1; fi
"$BIN" applytx "$T/chain" "$T/s1.signed" >/dev/null
if "$BIN" verify "$T/chain" "$T/s1.signed" >/dev/null 2>&1; then echo replay was not rejected >&2; exit 1; fi
# delegate 5B
make_tx "$T/alice" "$A" "$V" 5000000000 DELEGATE_BOND 1 "$T/d1"; "$BIN" applytx "$T/chain" "$T/d1.signed" >/dev/null
"$BIN" staking-status "$T/chain" "$V" > "$T/status1"
grep -q '^self_stake=20000000000$' "$T/status1"; grep -q '^delegated_to_me=5000000000$' "$T/status1"; grep -q '^validator_power=25000000000$' "$T/status1"
# unbond self 4B then claim exactly once
make_tx "$T/validator" "$V" "$V" 4000000000 STAKE_UNBOND 2 "$T/su"; "$BIN" applytx "$T/chain" "$T/su.signed" >/dev/null
make_tx "$T/validator" "$V" "$V" 0 STAKE_CLAIM 3 "$T/sc"; "$BIN" applytx "$T/chain" "$T/sc.signed" >/dev/null
if "$BIN" applytx "$T/chain" "$T/sc.signed" >/dev/null 2>&1; then echo double stake claim accepted >&2; exit 1; fi
# undelegate 2B then claim exactly once
make_tx "$T/alice" "$A" "$V" 2000000000 DELEGATE_UNBOND 2 "$T/du"; "$BIN" applytx "$T/chain" "$T/du.signed" >/dev/null
make_tx "$T/alice" "$A" "$V" 0 DELEGATE_CLAIM 3 "$T/dc"; "$BIN" applytx "$T/chain" "$T/dc.signed" >/dev/null
if "$BIN" applytx "$T/chain" "$T/dc.signed" >/dev/null 2>&1; then echo double delegation claim accepted >&2; exit 1; fi
"$BIN" staking-status "$T/chain" "$V" > "$T/status2"
grep -q '^self_stake=16000000000$' "$T/status2"; grep -q '^delegated_to_me=3000000000$' "$T/status2"; grep -q '^validator_power=19000000000$' "$T/status2"
"$BIN" supply-invariant "$T/chain" > "$T/invariant"
grep -q '^invariant_ok=1$' "$T/invariant"
# legacy direct paths must be dead
if "$BIN" stake "$T/chain" "$T/alice" 1 >/dev/null 2>&1; then echo legacy stake mutation still active >&2; exit 1; fi
if "$BIN" delegate "$T/chain" "$T/alice" "$V" 1 >/dev/null 2>&1; then echo legacy delegate mutation still active >&2; exit 1; fi
printf 'Phase 7.2.12 staking/delegation E2E PASS\n'
