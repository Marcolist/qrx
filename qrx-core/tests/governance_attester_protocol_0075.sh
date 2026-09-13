#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
GOVPASS='QRX-0075-Test-Governance-Passphrase!'
ATTPASS='QRX-0075-Test-Attester-Passphrase!'
"$Q" init-chain "$T/chain" >/dev/null
for i in 1 2 3 4 5; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" governance-keygen "$T/gov$i" "DEV_GOV_$i" >/dev/null; done
"$Q" governance-genesis-init "$T/chain" 3 "$T/gov1/governance.pub" "$T/gov2/governance.pub" "$T/gov3/governance.pub" "$T/gov4/governance.pub" "$T/gov5/governance.pub" >/dev/null
[[ $(grep -c '|ACTIVE$' "$T/chain/governance/governance_roots.db") -eq 5 ]]
! grep -RqiE 'PRIVATE KEY|governance\.key' "$T/chain/governance"
QRX_ATTESTER_PASSPHRASE="$ATTPASS" "$Q" privacy-attester-keygen "$T/cura" cura >/dev/null
"$Q" governance-attester-propose "$T/add.proposal" ATTESTER_ADD cura "$T/cura/attester.pub" verified-privacy,hidden-balance 0 >/dev/null
for i in 1 2 3; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" governance-sign "$T/gov$i" "$T/add.proposal" "$T/add.sig$i" >/dev/null; done
# Duplicate roots do not satisfy 3-of-5.
if "$Q" governance-apply "$T/chain" "$T/add.proposal" "$T/add.sig1" "$T/add.sig1" "$T/add.sig2" >/dev/null 2>&1; then echo 'FAIL duplicate signature accepted'; exit 1; fi
"$Q" governance-apply "$T/chain" "$T/add.proposal" "$T/add.sig1" "$T/add.sig2" "$T/add.sig3" >/dev/null
grep -q '^cura|.*|ACTIVE|' "$T/chain/privacy_attesters.db"
# Governance initialized => direct mutation must fail closed.
if "$Q" privacy-attester-disable "$T/chain" cura >/dev/null 2>&1; then echo 'FAIL direct mutation accepted'; exit 1; fi
# Exact proposal cannot be replayed.
if "$Q" governance-apply "$T/chain" "$T/add.proposal" "$T/add.sig1" "$T/add.sig2" "$T/add.sig3" >/dev/null 2>&1; then echo 'FAIL replay accepted'; exit 1; fi
# Tampering after signing invalidates the signatures.
cp "$T/add.proposal" "$T/tampered.proposal"
sed -i 's/hidden-balance/hidden-balance,evil-capability/' "$T/tampered.proposal"
if "$Q" governance-apply "$T/chain" "$T/tampered.proposal" "$T/add.sig1" "$T/add.sig2" "$T/add.sig3" >/dev/null 2>&1; then echo 'FAIL tampered proposal accepted'; exit 1; fi
# Mandatory protocol upgrade schedule and minimum transaction version.
"$Q" governance-protocol-propose "$T/proto.proposal" 8 0 3 4 VERIFIED_PRIVACY_V2,SHIELDED_PROOF_V4 >/dev/null
for i in 1 2 3; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" governance-sign "$T/gov$i" "$T/proto.proposal" "$T/proto.sig$i" >/dev/null; done
"$Q" governance-apply "$T/chain" "$T/proto.proposal" "$T/proto.sig1" "$T/proto.sig2" "$T/proto.sig3" >/dev/null
INFO=$($Q protocol-info "$T/chain")
grep -q '^active_protocol=8$' <<<"$INFO"
grep -q '^minimum_tx_version=3$' <<<"$INFO"
grep -q '^minimum_privacy_version=4$' <<<"$INFO"
grep -q '^update_required=false$' <<<"$INFO"
printf 'tx_version=2\n' > "$T/obsolete.tx"
if "$Q" verify "$T/chain" "$T/obsolete.tx" >"$T/obsolete.out" 2>&1; then echo 'FAIL obsolete tx accepted'; exit 1; fi
grep -q 'mandatory protocol update required' "$T/obsolete.out"
echo 'PASS: 0.0.7.5 3-of-5 developer governance, provider admission, replay/tamper rejection and protocol enforcement'
