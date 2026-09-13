#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build-phase71/qrx}
[[ -x "$Q" ]] || { echo "qrx binary missing: $Q" >&2; exit 1; }
T=$(mktemp -d /tmp/qrx-p71-gov.XXXXXX)
trap 'rm -rf "$T"' EXIT
PASS='phase71-wallet-pass'
GOVPASS='QRX-P71-GOVERNANCE-PASS'
ATTPASS='QRX-P71-ATTESTER-PASS'

"$Q" init-chain "$T/chain" 20 5000 2100000000000000 25000000 1000000000000 qrx-regtest 1 5152583731 QRX-Privacy-P71 >/dev/null
for i in 1 2 3 4 5; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" governance-keygen "$T/gov$i" "DEV_GOV_$i" >/dev/null; done
"$Q" governance-genesis-init "$T/chain" 3 "$T/gov1/governance.pub" "$T/gov2/governance.pub" "$T/gov3/governance.pub" "$T/gov4/governance.pub" "$T/gov5/governance.pub" >/dev/null
QRX_ATTESTER_PASSPHRASE="$ATTPASS" "$Q" privacy-attester-keygen "$T/att" cura >/dev/null
ATT_PUB=$(awk -F= '$1=="public_key_hex"{print $2}' "$T/att/attester.pub")
[[ ${#ATT_PUB} -eq 64 ]]

QRX_PASSPHRASE="$PASS" "$Q" seed-new "$T/payer" >/dev/null
ADDR=$(tr -d '\r\n' < "$T/payer/address.txt")
ED=$(openssl pkey -pubin -in "$T/payer/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
ML=$(base64 -w0 < "$T/payer/mldsa65_pub.pem")
"$Q" faucet "$T/chain" "$ADDR" 1000000 >/dev/null

make_gov_tx(){
  local payload="$1" raw="$2" signed="$3"
  "$Q" create-velocity-raw-tx "$T/chain" "$ADDR" "$ADDR" 0 "$ED" "$ML" PRIVACY_GOVERNANCE 6 5000 "$payload" > "$raw"
  QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/payer" "$T/chain" "$raw" "$signed" >/dev/null
}

"$Q" privacy-governance-propose-v2 "$T/chain" "$T/add.proposal" ATTESTER_ADD cura "$ATT_PUB" 0 >/dev/null
for i in 1 2 3; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" privacy-governance-sign-v2 "$T/chain" "$T/add.proposal" "$T/gov$i" "$T/add.sig$i" >/dev/null; done
PAYLOAD=$("$Q" privacy-governance-payload-v2 "$T/add.proposal" "$T/add.sig1" "$T/add.sig2" "$T/add.sig3" | sed -n 's/^payload=//p')
[[ -n "$PAYLOAD" ]]
make_gov_tx "$PAYLOAD" "$T/add.raw" "$T/add.signed"
"$Q" verify "$T/chain" "$T/add.signed" >/dev/null
"$Q" applytx "$T/chain" "$T/add.signed" >/dev/null

# Consensus attester state is now authoritative: V2 credential issuance succeeds.
QRX_ATTESTER_PASSPHRASE="$ATTPASS" "$Q" privacy-credential-issue-v2 "$T/chain" "$T/payer" cura "$T/att/attester.key" 4000 > "$T/cred.out"
grep -q '^status=issued-v2$' "$T/cred.out"

# Exact proposal replay must fail even with a fresh transaction/nonce.
make_gov_tx "$PAYLOAD" "$T/replay.raw" "$T/replay.signed"
if "$Q" verify "$T/chain" "$T/replay.signed" >/dev/null 2>&1; then echo 'FAIL: privacy governance proposal replay accepted' >&2; exit 1; fi

# Duplicate governance roots do not meet 3-of-5 threshold.
DUP=$("$Q" privacy-governance-payload-v2 "$T/add.proposal" "$T/add.sig1" "$T/add.sig1" "$T/add.sig2" | sed -n 's/^payload=//p')
make_gov_tx "$DUP" "$T/dup.raw" "$T/dup.signed"
if "$Q" verify "$T/chain" "$T/dup.signed" >/dev/null 2>&1; then echo 'FAIL: duplicate governance root counted twice' >&2; exit 1; fi

# Proposal tampering after signing invalidates all signatures.
cp "$T/add.proposal" "$T/tampered.proposal"
sed -i 's/public_key_hex=/public_key_hex=00/' "$T/tampered.proposal"
TAMP=$("$Q" privacy-governance-payload-v2 "$T/tampered.proposal" "$T/add.sig1" "$T/add.sig2" "$T/add.sig3" | sed -n 's/^payload=//p')
make_gov_tx "$TAMP" "$T/tampered.raw" "$T/tampered.signed"
if "$Q" verify "$T/chain" "$T/tampered.signed" >/dev/null 2>&1; then echo 'FAIL: tampered governance proposal accepted' >&2; exit 1; fi

# Chain-bound proposal cannot be transplanted to a second genesis.
"$Q" init-chain "$T/other" 20 5000 2100000000000000 25000000 1000000000000 qrx-regtest-other 1 5152583732 QRX-Privacy-P71-Other >/dev/null
"$Q" governance-genesis-init "$T/other" 3 "$T/gov1/governance.pub" "$T/gov2/governance.pub" "$T/gov3/governance.pub" "$T/gov4/governance.pub" "$T/gov5/governance.pub" >/dev/null
QRX_PASSPHRASE="$PASS" "$Q" seed-new "$T/otherpayer" >/dev/null
OADDR=$(tr -d '\r\n' < "$T/otherpayer/address.txt")
OED=$(openssl pkey -pubin -in "$T/otherpayer/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
OML=$(base64 -w0 < "$T/otherpayer/mldsa65_pub.pem")
"$Q" faucet "$T/other" "$OADDR" 1000000 >/dev/null
"$Q" create-velocity-raw-tx "$T/other" "$OADDR" "$OADDR" 0 "$OED" "$OML" PRIVACY_GOVERNANCE 6 5000 "$PAYLOAD" > "$T/cross.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/otherpayer" "$T/other" "$T/cross.raw" "$T/cross.signed" >/dev/null
if "$Q" verify "$T/other" "$T/cross.signed" >/dev/null 2>&1; then echo 'FAIL: cross-genesis privacy governance accepted' >&2; exit 1; fi

echo 'PASS: Phase 7.1 privacy governance 3-of-5 -> signed consensus tx -> QRXDB attester state -> V2 credential; replay/duplicate/tamper/cross-genesis rejected'
