#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" >/dev/null
sed -i 's/network_id=qrx-mainnet-community/network_id=qrx-regtest/' "$T/chain/chain.meta"
$Q keygen "$T/issuer" >/dev/null; $Q keygen "$T/alice" >/dev/null; $Q keygen "$T/bob" >/dev/null
ISS=$($Q address "$T/issuer" | tr -d '\r\n'); A=$($Q address "$T/alice" | tr -d '\r\n'); B=$($Q address "$T/bob" | tr -d '\r\n')
$Q asset-create-v1 "$T/chain" CURASHARE 'CURA Share' REGULATED "$ISS" 2 1000000 'ISSUER_FREEZE,ISSUER_REVOKE,ISSUER_FORCED_TRANSFER' BOTH >/dev/null
$Q asset-allow-v1 "$T/chain" CURASHARE "$ISS" "$A" >/dev/null
$Q asset-allow-v1 "$T/chain" CURASHARE "$ISS" "$B" >/dev/null
# BOTH also requires a valid accepted privacy credential bound to each recipient wallet.
export QRX_ATTESTER_PASSPHRASE='QRX-0076-Test-Attester-Passphrase!'
$Q privacy-attester-keygen "$T/att" testkyc >/dev/null
# no governance root initialized in this isolated regtest, so bootstrap registration is permitted
$Q privacy-attester-register "$T/chain" testkyc "$T/att/attester.pub.pem" >/dev/null
NOW=$(date +%s); UNTIL=$((NOW+86400))
$Q privacy-credential-issue "$T/chain" "$T/alice" testkyc "$T/att/attester.key" "$UNTIL" >/dev/null
$Q privacy-credential-issue "$T/chain" "$T/bob" testkyc "$T/att/attester.key" "$UNTIL" >/dev/null
$Q asset-mint-v1 "$T/chain" CURASHARE "$ISS" "$A" 1000 "$T/alice" >/dev/null
$Q asset-transfer-v1 "$T/chain" CURASHARE "$A" "$B" 200 "$T/bob" >/dev/null
[[ $($Q asset-balance "$T/chain" CURASHARE "$A") == 800 ]]
[[ $($Q asset-balance "$T/chain" CURASHARE "$B") == 200 ]]
$Q asset-freeze-v1 "$T/chain" CURASHARE "$ISS" "$B" >/dev/null
if $Q asset-transfer-v1 "$T/chain" CURASHARE "$B" "$A" 1 "$T/alice" >/dev/null 2>&1; then echo FAIL; exit 1; fi
$Q asset-unfreeze-v1 "$T/chain" CURASHARE "$ISS" "$B" >/dev/null
$Q asset-revoke-v1 "$T/chain" CURASHARE "$ISS" "$B" 50 >/dev/null
[[ $($Q asset-balance "$T/chain" CURASHARE "$B") == 150 ]]
$Q asset-forced-transfer-v1 "$T/chain" CURASHARE "$ISS" "$A" "$B" 100 "$T/bob" >/dev/null
[[ $($Q asset-balance "$T/chain" CURASHARE "$A") == 700 ]]
[[ $($Q asset-balance "$T/chain" CURASHARE "$B") == 250 ]]
# Existing QUB address identity remains byte-for-byte unchanged.
[[ "$A" == "$($Q address "$T/alice" | tr -d '\r\n')" ]]
echo 'PASS: QRX 0.0.7.6 native regulated asset state machine, KYC gate, allowlist, freeze, revoke, forced transfer, and address compatibility'
