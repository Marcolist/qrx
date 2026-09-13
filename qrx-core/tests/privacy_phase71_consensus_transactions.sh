#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build-phase71-final/qrx}
[[ -x "$Q" ]] || { echo "qrx binary missing: $Q" >&2; exit 1; }
T=$(mktemp -d /tmp/qrx-p71-privacy.XXXXXX)
trap 'rm -rf "$T"' EXIT
PASS='phase71-wallet-pass'; GOVPASS='QRX-P71-GOVERNANCE-PASS'; ATTPASS='QRX-P71-ATTESTER-PASS'
export QRX_PASSPHRASE="$PASS"
"$Q" init-chain "$T/chain" 20 5000 2100000000000000 25000000 1000000000000 qrx-regtest 1 5152583733 QRX-Privacy-P71-TX >/dev/null
for i in 1 2 3; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" governance-keygen "$T/gov$i" "DEV_GOV_$i" >/dev/null; done
"$Q" governance-genesis-init "$T/chain" 3 "$T/gov1/governance.pub" "$T/gov2/governance.pub" "$T/gov3/governance.pub" >/dev/null
QRX_ATTESTER_PASSPHRASE="$ATTPASS" "$Q" privacy-attester-keygen "$T/att" cura >/dev/null
ATT_PUB=$(awk -F= '$1=="public_key_hex"{print $2}' "$T/att/attester.pub")
wallet_new(){ QRX_PASSPHRASE="$PASS" "$Q" seed-new "$1" >/dev/null; }
wallet_info(){ local W="$1" P="$2"; eval "${P}_ADDR=\$(tr -d '\r\n' < '$W/address.txt')"; eval "${P}_ED=\$(openssl pkey -pubin -in '$W/ed25519_pub.pem' -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')"; eval "${P}_ML=\$(base64 -w0 < '$W/mldsa65_pub.pem')"; }
wallet_new "$T/a"; wallet_new "$T/b"; wallet_info "$T/a" A; wallet_info "$T/b" B
"$Q" faucet "$T/chain" "$A_ADDR" 100000 >/dev/null; "$Q" faucet "$T/chain" "$B_ADDR" 5000 >/dev/null
# Put attester into authoritative governance state.
"$Q" privacy-governance-propose-v2 "$T/chain" "$T/add.proposal" ATTESTER_ADD cura "$ATT_PUB" 0 >/dev/null
for i in 1 2 3; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" privacy-governance-sign-v2 "$T/chain" "$T/add.proposal" "$T/gov$i" "$T/add.sig$i" >/dev/null; done
GP=$($Q privacy-governance-payload-v2 "$T/add.proposal" "$T/add.sig1" "$T/add.sig2" "$T/add.sig3" | sed -n 's/^payload=//p')
"$Q" create-velocity-raw-tx "$T/chain" "$A_ADDR" "$A_ADDR" 0 "$A_ED" "$A_ML" PRIVACY_GOVERNANCE 6 5000 "$GP" > "$T/g.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/a" "$T/chain" "$T/g.raw" "$T/g.signed" >/dev/null
"$Q" applytx "$T/chain" "$T/g.signed" >/dev/null
QRX_ATTESTER_PASSPHRASE="$ATTPASS" "$Q" privacy-credential-issue-v2 "$T/chain" "$T/a" cura "$T/att/attester.key" 4000 >/dev/null
QRX_ATTESTER_PASSPHRASE="$ATTPASS" "$Q" privacy-credential-issue-v2 "$T/chain" "$T/b" cura "$T/att/attester.key" 4000 >/dev/null
BZ=$($Q shielded-address "$T/b" | awk -F= '$1=="shielded_address"{print $2}')
make_priv(){ local W=$1 ADDR=$2 ED=$3 ML=$4 ACT=$5 PUBAMT=$6 TO=$7 DEST=$8 OUT=$9 LANE=${10}; local P; P=$($Q prepare-privacy-payload "$T/chain" "$W" "$ACT" "$PUBAMT" "$DEST" | sed -n 's/^payload=//p'); "$Q" create-velocity-raw-tx "$T/chain" "$ADDR" "$TO" "$PUBAMT" "$ED" "$ML" "PRIVACY_${ACT}" "$LANE" 5000 "$P" > "$OUT.raw"; QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$W" "$T/chain" "$OUT.raw" "$OUT.signed" >/dev/null; echo "$P" > "$OUT.payload"; }
A0=$($Q balance "$T/chain" "$A_ADDR")
make_priv "$T/a" "$A_ADDR" "$A_ED" "$A_ML" SHIELD 10000 "$A_ADDR" - "$T/shield" 6
"$Q" verify "$T/chain" "$T/shield.signed" >/dev/null; "$Q" applytx "$T/chain" "$T/shield.signed" >/dev/null
[[ $($Q balance "$T/chain" "$A_ADDR") -eq $((A0-11000)) ]]
[[ $($Q privacy-consensus-balance "$T/chain" "$T/a") -eq 10000 ]]
# Credential is consensus-mandatory, not a GUI-only wrapper.
P=$(cat "$T/shield.payload"); NOCRED=$(printf '%s' "$P" | sed -E 's/credential_b64=[^;]*;//')
"$Q" create-velocity-raw-tx "$T/chain" "$A_ADDR" "$A_ADDR" 10000 "$A_ED" "$A_ML" PRIVACY_SHIELD 7 5000 "$NOCRED" > "$T/nocred.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/a" "$T/chain" "$T/nocred.raw" "$T/nocred.signed" >/dev/null
if "$Q" verify "$T/chain" "$T/nocred.signed" >/dev/null 2>&1; then echo 'FAIL: PRIVACY_SHIELD without credential accepted' >&2; exit 1; fi
# Private transfer consumes a nullifier and creates recipient + change notes.
TP=$($Q prepare-privacy-payload "$T/chain" "$T/a" TRANSFER 3000 "$BZ" | sed -n 's/^payload=//p')
"$Q" create-velocity-raw-tx "$T/chain" "$A_ADDR" "$A_ADDR" 0 "$A_ED" "$A_ML" PRIVACY_TRANSFER 6 5000 "$TP" > "$T/t.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/a" "$T/chain" "$T/t.raw" "$T/t.signed" >/dev/null
"$Q" verify "$T/chain" "$T/t.signed" >/dev/null; "$Q" applytx "$T/chain" "$T/t.signed" >/dev/null
[[ $($Q privacy-consensus-balance "$T/chain" "$T/a") -eq 7000 ]]
[[ $($Q privacy-consensus-balance "$T/chain" "$T/b") -eq 3000 ]]
# Fresh transaction with the already-spent proof/nullifier is rejected.
"$Q" create-velocity-raw-tx "$T/chain" "$A_ADDR" "$A_ADDR" 0 "$A_ED" "$A_ML" PRIVACY_TRANSFER 7 5000 "$TP" > "$T/replay.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/a" "$T/chain" "$T/replay.raw" "$T/replay.signed" >/dev/null
if QRX_PRIVACY_DEBUG=1 "$Q" verify "$T/chain" "$T/replay.signed" >"$T/replay.out" 2>"$T/replay.err"; then echo 'FAIL: spent privacy nullifier replay accepted' >&2; exit 1; fi
grep -qi 'key-image-already-spent\|privacy' "$T/replay.err"
# Unshield pays fee and credits public QUB in the same authoritative applytx.
B0=$($Q balance "$T/chain" "$B_ADDR")
UP=$($Q prepare-privacy-payload "$T/chain" "$T/b" UNSHIELD 2000 "$B_ADDR" | sed -n 's/^payload=//p')
"$Q" create-velocity-raw-tx "$T/chain" "$B_ADDR" "$B_ADDR" 2000 "$B_ED" "$B_ML" PRIVACY_UNSHIELD 6 5000 "$UP" > "$T/u.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/b" "$T/chain" "$T/u.raw" "$T/u.signed" >/dev/null
"$Q" verify "$T/chain" "$T/u.signed" >/dev/null; "$Q" applytx "$T/chain" "$T/u.signed" >/dev/null
[[ $($Q balance "$T/chain" "$B_ADDR") -eq $((B0+1000)) ]]
[[ $($Q privacy-consensus-balance "$T/chain" "$T/b") -eq 1000 ]]
# Proof/credential bundle is bound to genesis/chain/protocol and cannot transplant.
"$Q" init-chain "$T/other" 20 5000 2100000000000000 25000000 1000000000000 qrx-regtest-other 1 5152583734 QRX-Privacy-P71-Other >/dev/null
"$Q" faucet "$T/other" "$A_ADDR" 100000 >/dev/null
"$Q" create-velocity-raw-tx "$T/other" "$A_ADDR" "$A_ADDR" 0 "$A_ED" "$A_ML" PRIVACY_TRANSFER 6 5000 "$TP" > "$T/cross.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/a" "$T/other" "$T/cross.raw" "$T/cross.signed" >/dev/null
if "$Q" verify "$T/other" "$T/cross.signed" >/dev/null 2>&1; then echo 'FAIL: cross-genesis privacy proof accepted' >&2; exit 1; fi
echo 'PASS: Phase 7.1 PRIVACY_SHIELD -> PRIVACY_TRANSFER -> PRIVACY_UNSHIELD authoritative QRXDB lifecycle; credential/nullifier/cross-genesis enforcement'
