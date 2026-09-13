#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build-phase72/qrx}
[[ -x "$Q" ]] || { echo "qrx binary missing: $Q" >&2; exit 1; }
T=$(mktemp -d /tmp/qrx-p72-kyc.XXXXXX)
trap 'rm -rf "$T"' EXIT
PASS='phase72-wallet-pass'
GOVPASS='QRX-P72-GOVERNANCE-PASS'
KYCPASS='QRX-P72-KYC-PROVIDER-PASS'

"$Q" init-chain "$T/chain" 20 5000 2100000000000000 25000000 1000000000000 qrx-regtest 1 5152583731 QRX-P72 >/dev/null
for i in 1 2 3 4 5; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" governance-keygen "$T/gov$i" "DEV_GOV_$i" >/dev/null; done
"$Q" governance-genesis-init "$T/chain" 3 "$T/gov1/governance.pub" "$T/gov2/governance.pub" "$T/gov3/governance.pub" "$T/gov4/governance.pub" "$T/gov5/governance.pub" >/dev/null
QRX_KYC_PROVIDER_PASSPHRASE="$KYCPASS" "$Q" kyc-provider-keygen "$T/provider" CURA >/dev/null
KYC_PUB=$(awk -F= '$1=="public_key_hex"{print $2}' "$T/provider/kyc-provider.pub")
[[ ${#KYC_PUB} -eq 64 ]]

for w in provider attacker target; do QRX_PASSPHRASE="$PASS" "$Q" seed-new "$T/$w" >/dev/null; done
ADDR=$(tr -d '\r\n' < "$T/provider/address.txt")
ATTACKER=$(tr -d '\r\n' < "$T/attacker/address.txt")
TARGET=$(tr -d '\r\n' < "$T/target/address.txt")
ED=$(openssl pkey -pubin -in "$T/provider/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
ML=$(base64 -w0 < "$T/provider/mldsa65_pub.pem")
AED=$(openssl pkey -pubin -in "$T/attacker/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
AML=$(base64 -w0 < "$T/attacker/mldsa65_pub.pem")
"$Q" faucet "$T/chain" "$ADDR" 100000000000 >/dev/null
"$Q" faucet "$T/chain" "$ATTACKER" 100000000000 >/dev/null

make_gov_tx(){
  local payload="$1" raw="$2" signed="$3"
  "$Q" create-velocity-raw-tx "$T/chain" "$ADDR" "$ADDR" 0 "$ED" "$ML" PRIVACY_GOVERNANCE 6 5000 "$payload" > "$raw"
  QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/provider" "$T/chain" "$raw" "$signed" >/dev/null
}
apply_proposal(){
  local proposal="$1" prefix="$2"
  for i in 1 2 3; do QRX_GOV_PASSPHRASE="$GOVPASS" "$Q" privacy-governance-sign-v2 "$T/chain" "$proposal" "$T/gov$i" "$T/${prefix}.sig$i" >/dev/null; done
  local payload
  payload=$("$Q" privacy-governance-payload-v2 "$proposal" "$T/${prefix}.sig1" "$T/${prefix}.sig2" "$T/${prefix}.sig3" | sed -n 's/^payload=//p')
  make_gov_tx "$payload" "$T/${prefix}.raw" "$T/${prefix}.signed"
  "$Q" verify "$T/chain" "$T/${prefix}.signed" >/dev/null
  "$Q" applytx "$T/chain" "$T/${prefix}.signed" >/dev/null
}

"$Q" kyc-provider-propose-v2 "$T/chain" "$T/add.proposal" KYC_PROVIDER_ADD CURA "$ADDR" "$KYC_PUB" 'KYC,AML,AGE,ACCREDITED' 0 >/dev/null
apply_proposal "$T/add.proposal" add
"$Q" kyc-provider-info "$T/chain" CURA > "$T/info"
grep -q '^status=ACTIVE$' "$T/info"
grep -q "^authority_address=$ADDR$" "$T/info"
grep -q '^qualifier=#KYC_CURA$' "$T/info"

# Attacker cannot create governance-bound KYC qualifier.
P='asset=#KYC_CURA;kind=QUALIFIER;qty=1;units=0;reissuable=0;metadata=kyc;verifier=true;capabilities=KYC;max_supply=1'
"$Q" create-velocity-raw-tx "$T/chain" "$ATTACKER" "$ATTACKER" 0 "$AED" "$AML" ASSET_ISSUE 7 5000 "$P" > "$T/bad.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/attacker" "$T/chain" "$T/bad.raw" "$T/bad.signed" >/dev/null
if "$Q" applytx "$T/chain" "$T/bad.signed" >/dev/null 2>&1; then echo 'FAIL: unauthorized KYC qualifier issue accepted' >&2; exit 1; fi

# Authorized provider creates its KYC qualifier and tags target.
"$Q" create-velocity-raw-tx "$T/chain" "$ADDR" "$ADDR" 0 "$ED" "$ML" ASSET_ISSUE 7 5000 "$P" > "$T/issue.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/provider" "$T/chain" "$T/issue.raw" "$T/issue.signed" >/dev/null
"$Q" applytx "$T/chain" "$T/issue.signed" >/dev/null
TP="qualifier=#KYC_CURA;address=$TARGET"
"$Q" create-velocity-raw-tx "$T/chain" "$ADDR" "$ADDR" 0 "$ED" "$ML" ASSET_TAG 7 5000 "$TP" > "$T/tag.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/provider" "$T/chain" "$T/tag.raw" "$T/tag.signed" >/dev/null
"$Q" applytx "$T/chain" "$T/tag.signed" >/dev/null
"$Q" asset-tag-check "$T/chain" '#KYC_CURA' "$TARGET" | grep -q '^tagged=true$'

# Restricted asset verifier consumes active provider tag.
RP='asset=$EUROTEST;kind=RESTRICTED;qty=100;units=2;reissuable=1;metadata=regulated;verifier=#KYC_CURA;capabilities=ISSUER_FREEZE,ISSUER_REVOKE,ISSUER_FORCED_TRANSFER;max_supply=100000'
"$Q" create-velocity-raw-tx "$T/chain" "$ADDR" "$TARGET" 0 "$ED" "$ML" ASSET_ISSUE 7 5000 "$RP" > "$T/reg.raw"
QRX_PASSPHRASE="$PASS" "$Q" signrawtransactionwithwallet "$T/provider" "$T/chain" "$T/reg.raw" "$T/reg.signed" >/dev/null
"$Q" applytx "$T/chain" "$T/reg.signed" >/dev/null
"$Q" asset-restriction-check "$T/chain" '$EUROTEST' "$TARGET" | grep -q '^eligible=true$'

# Disable provider: existing tags instantly stop satisfying verifier.
"$Q" kyc-provider-propose-v2 "$T/chain" "$T/dis.proposal" KYC_PROVIDER_DISABLE CURA - - - 0 >/dev/null
apply_proposal "$T/dis.proposal" dis
"$Q" kyc-provider-info "$T/chain" CURA | grep -q '^status=DISABLED$'
"$Q" asset-tag-check "$T/chain" '#KYC_CURA' "$TARGET" | grep -q '^tagged=false$'
"$Q" asset-restriction-check "$T/chain" '$EUROTEST' "$TARGET" | grep -q '^eligible=false$'

echo 'PASS: Phase 7.2 developer-threshold KYC provider registry controls #KYC_PROVIDER issuance/tagging and regulated-asset eligibility; disable revokes provider tags globally'
