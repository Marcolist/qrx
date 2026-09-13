#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d /tmp/qrx-generals-p41.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P41 >/dev/null
QRX_PASSPHRASE=test $Q seed-new "$T/alice" >/dev/null
A=$(tr -d '\r\n' < "$T/alice/address.txt")
ED=$(openssl pkey -pubin -in "$T/alice/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
ML=$(base64 -w0 < "$T/alice/mldsa65_pub.pem")
apply(){ local type=$1 payload=$2 tag=$3; $Q create-velocity-raw-tx "$T/chain" "$A" "$A" 0 "$ED" "$ML" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"; QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null; $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }
$Q faucet "$T/chain" "$A" 10000000000 >/dev/null
apply GAME_JOIN 'display_name=alice' join
apply GAME_SEASON_JOIN 'season_id=1' season
$Q generals-logistics-info "$T/chain" "$A" 1 | grep -q '^materials=1500$'
HQ=$($Q generals-player-army-info "$T/chain" "$A" | sed -n 's/^hq_unit=//p')
X=$($Q generals-unit-info "$T/chain" "$HQ" | sed -n 's/^x=//p'); Y=$($Q generals-unit-info "$T/chain" "$HQ" | sed -n 's/^y=//p')
apply GAME_INFRA_BUILD "infra_type=FACTORY;x=$X;y=$Y" factory
$Q generals-infra-info "$T/chain" 1 "$X" "$Y" | grep -q '^type=FACTORY$'
$Q generals-infra-info "$T/chain" 1 "$X" "$Y" | grep -q "^owner=$A$"
$Q generals-logistics-info "$T/chain" "$A" 1 | grep -q '^materials=1100$'
apply GAME_PRODUCE_UNIT "unit_type=TANK;x=$X;y=$Y" tank
$Q generals-logistics-info "$T/chain" "$A" 1 | grep -q '^supply=1830$'
# duplicate infrastructure on same tile must fail atomically
before=$($Q generals-logistics-info "$T/chain" "$A" 1 | sed -n 's/^materials=//p')
set +e; apply GAME_INFRA_BUILD "infra_type=ROAD;x=$X;y=$Y" dup >/dev/null 2>&1; r=$?; set -e
[[ $r -ne 0 ]]
after=$($Q generals-logistics-info "$T/chain" "$A" 1 | sed -n 's/^materials=//p'); [[ "$before" == "$after" ]]
echo 'PASS: QRX Generals 0.0.7.7 Phase 4.1 supply, production and strategic infrastructure: deterministic logistics resources, owned-territory factory construction, unit production, atomic resource debit and duplicate-build rejection'
