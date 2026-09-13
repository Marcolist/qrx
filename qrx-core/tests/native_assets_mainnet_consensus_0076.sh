#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build-mainnet/qrx}
T=$(mktemp -d /tmp/qrx-assets-mainnet.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583036 QRX-Asset-Test >/dev/null
QRX_PASSPHRASE=test $Q seed-new "$T/iss" >/dev/null
QRX_PASSPHRASE=test $Q seed-new "$T/alice" >/dev/null
QRX_PASSPHRASE=test $Q seed-new "$T/bob" >/dev/null
ISS=$(tr -d '\r\n' < "$T/iss/address.txt"); A=$(tr -d '\r\n' < "$T/alice/address.txt"); B=$(tr -d '\r\n' < "$T/bob/address.txt")
pubs(){ local w=$1; openssl pkey -pubin -in "$w/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n'; }
ml(){ base64 -w0 < "$1/mldsa65_pub.pem"; }
IE=$(pubs "$T/iss"); IM=$(ml "$T/iss"); AE=$(pubs "$T/alice"); AM=$(ml "$T/alice")
$Q faucet "$T/chain" "$ISS" 120000000000 >/dev/null; $Q faucet "$T/chain" "$A" 50000000 >/dev/null; $Q faucet "$T/chain" "$B" 50000000 >/dev/null
mkapply(){ local w=$1 from=$2 to=$3 ed=$4 mlp=$5 type=$6 lane=$7 payload=$8 tag=$9; $Q create-velocity-raw-tx "$T/chain" "$from" "$to" 0 "$ed" "$mlp" "$type" "$lane" 1000 "$payload" > "$T/$tag.raw"; QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$w" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null; $Q verify "$T/chain" "$T/$tag.signed" >/dev/null; $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }
# Ravencoin-style main asset: reissuable=true, units=2, metadata, owner token auto-created.
mkapply "$T/iss" "$ISS" "$A" "$IE" "$IM" ASSET_ISSUE 7 'asset=ACME;kind=MAIN;qty=10000;units=2;reissuable=1;metadata=0123456789abcdef;verifier=true;capabilities=NONE;max_supply=20000' issue
[[ "$($Q asset-balance "$T/chain" ACME "$A")" == 10000 ]]
[[ "$($Q asset-balance "$T/chain" 'ACME!' "$ISS")" == 1 ]]
# Reissue/remint + units may only increase; then permanently disable reissuance.
mkapply "$T/iss" "$ISS" "$A" "$IE" "$IM" ASSET_REISSUE 7 'asset=ACME;qty=5000;units=4;reissuable=0;metadata=abcdef;verifier=' reissue
[[ "$($Q asset-balance "$T/chain" ACME "$A")" == 15000 ]]
$Q create-velocity-raw-tx "$T/chain" "$ISS" "$A" 0 "$IE" "$IM" ASSET_REISSUE 7 1000 'asset=ACME;qty=1;units=4;reissuable=1;metadata=-;verifier=' > "$T/reissue2.raw"
QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/iss" "$T/chain" "$T/reissue2.raw" "$T/reissue2.signed" >/dev/null
if $Q applytx "$T/chain" "$T/reissue2.signed" >/dev/null 2>&1; then echo 'FAIL: reissuability resurrected'; exit 1; fi
# Sub asset and unique asset require parent ownership.
mkapply "$T/iss" "$ISS" "$A" "$IE" "$IM" ASSET_ISSUE 7 'asset=ACME/CLASS_A;kind=SUB;qty=100;units=0;reissuable=1;metadata=-;verifier=true;capabilities=NONE;max_supply=1000' sub
mkapply "$T/iss" "$ISS" "$A" "$IE" "$IM" ASSET_ISSUE 7 'asset=ACME#CERT001;kind=UNIQUE;qty=1;units=0;reissuable=0;metadata=cert-hash;verifier=true;capabilities=NONE;max_supply=1' uniq
# Qualifier + tag + restricted asset verifier, then transfer eligibility enforcement.
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_ISSUE 7 'asset=#KYC;kind=QUALIFIER;qty=1;units=0;reissuable=0;metadata=kyc-provider;verifier=true;capabilities=NONE;max_supply=1' qual
mkapply "$T/iss" "$ISS" "$B" "$IE" "$IM" ASSET_TAG 7 "qualifier=#KYC;address=$B" tagb
mkapply "$T/iss" "$ISS" "$B" "$IE" "$IM" ASSET_ISSUE 7 'asset=$SEC;kind=RESTRICTED;qty=1000;units=2;reissuable=1;metadata=regulated;verifier=KYC;capabilities=ISSUER_REVOKE,ISSUER_FORCED_TRANSFER;max_supply=10000' sec
[[ "$($Q asset-balance "$T/chain" '$SEC' "$B")" == 1000 ]]
# Alice is not tagged: transfer to Alice must fail atomically.
$Q create-velocity-raw-tx "$T/chain" "$B" "$A" 0 "$(pubs "$T/bob")" "$(ml "$T/bob")" ASSET_TRANSFER 8 1000 'asset=$SEC;qty=10' > "$T/bad.raw"
QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/bob" "$T/chain" "$T/bad.raw" "$T/bad.signed" >/dev/null
if $Q applytx "$T/chain" "$T/bad.signed" >/dev/null 2>&1; then echo 'FAIL: verifier bypass'; exit 1; fi
# Tag Alice then transfer succeeds.
mkapply "$T/iss" "$ISS" "$A" "$IE" "$IM" ASSET_TAG 7 "qualifier=#KYC;address=$A" taga
mkapply "$T/bob" "$B" "$A" "$(pubs "$T/bob")" "$(ml "$T/bob")" ASSET_TRANSFER 8 'asset=$SEC;qty=10' txsec
[[ "$($Q asset-balance "$T/chain" '$SEC' "$A")" == 10 ]]
# Global freeze blocks transfers.
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_GLOBAL_FREEZE 7 'asset=$SEC' gf
$Q create-velocity-raw-tx "$T/chain" "$A" "$B" 0 "$AE" "$AM" ASSET_TRANSFER 9 1000 'asset=$SEC;qty=1' > "$T/frozen.raw"
QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/frozen.raw" "$T/frozen.signed" >/dev/null
if $Q applytx "$T/chain" "$T/frozen.signed" >/dev/null 2>&1; then echo 'FAIL: global freeze bypass'; exit 1; fi
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_GLOBAL_UNFREEZE 7 'asset=$SEC' guf

# Sub-qualifier and message channel parity.
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_ISSUE 7 'asset=#KYC/#EU;kind=SUBQUALIFIER;qty=1;units=0;reissuable=0;metadata=eu-kyc;verifier=true;capabilities=NONE;max_supply=1' subqual
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_ISSUE 7 'asset=ACME~NEWS;kind=CHANNEL;qty=1;units=0;reissuable=0;metadata=channel;verifier=true;capabilities=NONE;max_supply=1' channel
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_BROADCAST 7 'asset=ACME~NEWS;message=Quarterly_Update;expire_time=9999999999' broadcast
# Per-address restricted freeze/unfreeze.
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_FREEZE_ADDRESS 7 "asset=\$SEC;address=$A" freezeA
$Q create-velocity-raw-tx "$T/chain" "$A" "$B" 0 "$AE" "$AM" ASSET_TRANSFER 9 1000 'asset=$SEC;qty=1' > "$T/faddr.raw"
QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/faddr.raw" "$T/faddr.signed" >/dev/null
if $Q applytx "$T/chain" "$T/faddr.signed" >/dev/null 2>&1; then echo 'FAIL: address freeze bypass'; exit 1; fi
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_UNFREEZE_ADDRESS 7 "asset=\$SEC;address=$A" unfreezeA
# Issuer revoke and forced transfer are capability-gated and consensus atomic.
mkapply "$T/iss" "$ISS" "$ISS" "$IE" "$IM" ASSET_REVOKE 7 "asset=\$SEC;source=$A;qty=2" revoke
[[ "$($Q asset-balance "$T/chain" '$SEC' "$A")" == 8 ]]
mkapply "$T/iss" "$ISS" "$B" "$IE" "$IM" ASSET_FORCED_TRANSFER 7 "asset=\$SEC;source=$A;qty=3" forced
[[ "$($Q asset-balance "$T/chain" '$SEC' "$A")" == 5 ]]
[[ "$($Q asset-balance "$T/chain" '$SEC' "$B")" == 993 ]]
# Self transfer must be a balance no-op (fee only), never mint.
BEFORE=$($Q asset-balance "$T/chain" '$SEC' "$B")
mkapply "$T/bob" "$B" "$B" "$(pubs "$T/bob")" "$(ml "$T/bob")" ASSET_TRANSFER 8 'asset=$SEC;qty=10' selftx
AFTER=$($Q asset-balance "$T/chain" '$SEC' "$B")
[[ "$BEFORE" == "$AFTER" ]]

# Introspection: consensus metadata exposes one-way reissuability and burn accounting.
$Q asset-info-v1 "$T/chain" ACME | grep -q '^reissuable=0$'
$Q list-assets "$T/chain" | grep -q 'asset=ACME status=active consensus_asset=true'
BURNED=$($Q asset-burned-fees "$T/chain")
[[ "$BURNED" -gt 0 ]]
$Q asset-tag-check "$T/chain" '#KYC' "$A" | grep -q '^tagged=true$'
$Q asset-restriction-check "$T/chain" '$SEC' "$A" | grep -q '^global_frozen=false$'

# Existing address remains unchanged.
[[ "$A" == "$(tr -d '\r\n' < "$T/alice/address.txt")" ]]
echo 'PASS: 0.0.7.6 consensus native assets: issue/reissue lock/sub/unique/owner/qualifier/restricted/tag/global-freeze/address-compatibility'
