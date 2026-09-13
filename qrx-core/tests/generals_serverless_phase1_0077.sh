#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d /tmp/qrx-generals-p1.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583036 QRX-Generals-P1 >/dev/null
QRX_PASSPHRASE=test $Q seed-new "$T/alice" >/dev/null
A=$(tr -d '\r\n' < "$T/alice/address.txt")
pub(){ openssl pkey -pubin -in "$1/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n'; }
ml(){ base64 -w0 < "$1/mldsa65_pub.pem"; }
AE=$(pub "$T/alice"); AM=$(ml "$T/alice")
# 100 QUB starting funds. Phase-1 defaults: join 10 QUB, clan create 50 QUB.
$Q faucet "$T/chain" "$A" 10000000000 >/dev/null
mkapply(){ local type=$1 amount=$2 payload=$3 tag=$4; $Q create-velocity-raw-tx "$T/chain" "$A" "$A" "$amount" "$AE" "$AM" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"; QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null; $Q verify "$T/chain" "$T/$tag.signed" >/dev/null; $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }

mkapply GAME_JOIN 0 'display_name=Alice General' join
PINFO=$($Q generals-player-info "$T/chain" "$A")
echo "$PINFO" | grep -q '^status=active$'
GENERAL=$(echo "$PINFO" | sed -n 's/^general_asset=//p')
[[ "$GENERAL" == GENERAL#* ]]
[[ "$($Q asset-balance "$T/chain" "$GENERAL" "$A")" == 1 ]]
$Q asset-info-v1 "$T/chain" "$GENERAL" | grep -q '^kind=UNIQUE$'
$Q asset-info-v1 "$T/chain" "$GENERAL" | grep -q '^issuer=QRX_GENERALS_PROTOCOL$'

# A wallet/player cannot GAME_JOIN twice and mint duplicate Generals.
TREASURY_BEFORE_DUP=$($Q generals-treasury-info "$T/chain" | sed -n 's/^balance_atoms=//p')
$Q create-velocity-raw-tx "$T/chain" "$A" "$A" 0 "$AE" "$AM" GAME_JOIN 12 1000 'display_name=Duplicate' 0 > "$T/dup.raw"
QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/dup.raw" "$T/dup.signed" >/dev/null
if $Q applytx "$T/chain" "$T/dup.signed" >/dev/null 2>&1; then echo 'FAIL: duplicate GAME_JOIN accepted'; exit 1; fi
[[ "$($Q generals-treasury-info "$T/chain" | sed -n 's/^balance_atoms=//p')" == "$TREASURY_BEFORE_DUP" ]]

# Generic ASSET_TRANSFER is forbidden for GENERAL# so game and asset ownership cannot diverge.
$Q create-velocity-raw-tx "$T/chain" "$A" "$A" 0 "$AE" "$AM" ASSET_TRANSFER 13 1000 "asset=$GENERAL;qty=1" 0 > "$T/generic.raw"
QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/generic.raw" "$T/generic.signed" >/dev/null
if $Q applytx "$T/chain" "$T/generic.signed" >/dev/null 2>&1; then echo 'FAIL: reserved General used generic transfer'; exit 1; fi

mkapply GAME_CLAN_CREATE 0 'clan_name=First Legion;clan_tag=FL' clan
PINFO=$($Q generals-player-info "$T/chain" "$A")
CLAN=$(echo "$PINFO" | sed -n 's/^clan_id=//p')
[[ "$CLAN" == CLAN-* ]]
$Q generals-clan-info "$T/chain" "$CLAN" | grep -q '^name=First Legion$'
$Q generals-clan-info "$T/chain" "$CLAN" | grep -q '^tag=FL$'
$Q generals-clan-info "$T/chain" "$CLAN" | grep -q "^founder=$A$"

# Voluntary 5 QUB treasury funding.
mkapply GAME_TREASURY_FUND 500000000 'memo=phase1-test' fund
TINFO=$($Q generals-treasury-info "$T/chain")
echo "$TINFO" | grep -q '^balance_atoms=6500000000$'
echo "$TINFO" | grep -q '^balance_qub=65.00000000$'

# 100 QUB - 10 join - 50 clan - 5 fund = 35 QUB. No normal fee in this test.
[[ "$($Q balance "$T/chain" "$A")" == 3500000000 ]]
# Existing wallet address remains byte-for-byte unchanged.
[[ "$A" == "$(tr -d '\r\n' < "$T/alice/address.txt")" ]]
echo 'PASS: QRX Generals 0.0.7.7 Phase 1 serverless consensus: player identity, native unique General, GAME_JOIN, clan creation, treasury funding, WAL atomicity, address compatibility'
