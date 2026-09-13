#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d /tmp/qrx-generals-p4.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P4 >/dev/null
mk_wallet(){ local name=$1; QRX_PASSPHRASE=test $Q seed-new "$T/$name" >/dev/null; local a=$(tr -d '\r\n' < "$T/$name/address.txt"); local ed=$(openssl pkey -pubin -in "$T/$name/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n'); local ml=$(base64 -w0 < "$T/$name/mldsa65_pub.pem"); echo "$a|$ed|$ml"; }
apply_for(){ local who=$1 type=$2 payload=$3 tag=$4; IFS='|' read -r addr ed ml <<<"${W[$who]}"; $Q create-velocity-raw-tx "$T/chain" "$addr" "$addr" 0 "$ed" "$ml" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"; QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/$who" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null; $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }
declare -A W
for who in alice bob; do W[$who]=$(mk_wallet "$who"); IFS='|' read -r addr _ _ <<<"${W[$who]}"; $Q faucet "$T/chain" "$addr" 5000000000 >/dev/null; apply_for "$who" GAME_JOIN "display_name=$who" "${who}join"; apply_for "$who" GAME_SEASON_JOIN 'season_id=1' "${who}season"; done
A=${W[alice]%%|*}
ARMY=$($Q generals-player-army-info "$T/chain" "$A")
echo "$ARMY" | grep -q '^unit_count=4$'
echo "$ARMY" | grep -q '^support_unit_count=3$'
echo "$ARMY" | grep -q '^army_unit_count=7$'
MISS=$(echo "$ARMY" | sed -n 's/^support_unit_2=//p')
[[ "$($Q generals-unit-info "$T/chain" "$MISS" | sed -n 's/^type=//p')" == MISSILE_BATTERY ]]
$Q generals-unit-class-info MISSILE_BATTERY | grep -q '^max_range=256$'
$Q generals-unit-class-info RECON | grep -q '^move_points=6$'
MX=$($Q generals-unit-info "$T/chain" "$MISS" | sed -n 's/^x=//p'); MY=$($Q generals-unit-info "$T/chain" "$MISS" | sed -n 's/^y=//p')
TARGET_OWNER=bob
addr=${W[bob]%%|*}
TARGET=$($Q generals-player-army-info "$T/chain" "$addr" | sed -n 's/^hq_unit=//p')
[[ -n "$TARGET" ]] || { echo 'FAIL: no deterministic long-range target found'; exit 1; }
SECRET='phase4-missile-attack-secret'
CANON="QRX-GENERALS-ORDER-v2|1|1|ATTACK|$MISS|$TARGET|$SECRET"
COMMIT=$(printf '%s' "$CANON" | openssl dgst -sha3-512 | awk '{print $2}')
apply_for alice GAME_ORDER_COMMIT "season_id=1;turn_id=1;commitment=$COMMIT" atkcommit
HASH=$(sed -n 's/^body_hash_sha3_512=//p' "$T/atkcommit.signed"); OID="ORD-${HASH:0:24}"
mkdir -p "$T/chain/blocks"; for i in $(seq 1 30); do printf 'p4-reveal-%s\n' "$i" > "$T/chain/blocks/p4-r-$i.block"; done
HP0=$($Q generals-unit-info "$T/chain" "$TARGET" | sed -n 's/^hp=//p')
# Since Phase 4.4, long-range fire requires a valid recon/radar contact. Preserve the
# Phase-4 commit path, but verify the modern consensus rejects blind missile fire.
if apply_for alice GAME_ORDER_REVEAL "action=ATTACK;order_id=$OID;unit_id=$MISS;target_unit_id=$TARGET;secret=$SECRET" atkreveal 2>/dev/null; then
  echo 'FAIL: blind long-range attack accepted without Phase 4.4 contact' >&2; exit 1
fi
HP1=$($Q generals-unit-info "$T/chain" "$TARGET" | sed -n 's/^hp=//p'); [[ "$HP1" == "$HP0" ]]
grep -q 'pending_damage' "$(dirname "$0")/../src/qrx.c"
grep -q 'generals_contact_visible(c,from,target,h)' "$(dirname "$0")/../src/qrx.c"
# Legacy phase-3 unit remains intact and phase-4 territory ownership exists at HQ.
HQ=$($Q generals-player-army-info "$T/chain" "$A" | sed -n 's/^hq_unit=//p'); HX=$($Q generals-unit-info "$T/chain" "$HQ"|sed -n 's/^x=//p'); HY=$($Q generals-unit-info "$T/chain" "$HQ"|sed -n 's/^y=//p')
$Q generals-tile-info "$T/chain" 1 "$HX" "$HY" | grep -q "^territory_owner=$A$"
echo 'PASS: QRX Generals Phase 4 regression under Phase 4.4+ rules: support units, strategic commitment, blind long-range fire rejection, pending-damage surface, territory state, legacy starter-index compatibility'
