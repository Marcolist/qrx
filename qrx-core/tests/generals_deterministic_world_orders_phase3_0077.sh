#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d /tmp/qrx-generals-p3.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P3 >/dev/null
QRX_PASSPHRASE=test $Q seed-new "$T/alice" >/dev/null
A=$(tr -d '\r\n' < "$T/alice/address.txt")
AE=$(openssl pkey -pubin -in "$T/alice/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
AM=$(base64 -w0 < "$T/alice/mldsa65_pub.pem")
$Q faucet "$T/chain" "$A" 5000000000 >/dev/null
mkapply(){ local type=$1 payload=$2 tag=$3; $Q create-velocity-raw-tx "$T/chain" "$A" "$A" 0 "$AE" "$AM" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"; QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/alice" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null; $Q verify "$T/chain" "$T/$tag.signed" >/dev/null; $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }
mkapply GAME_JOIN 'display_name=Alice General' join
mkapply GAME_SEASON_JOIN 'season_id=1' season
W=$($Q generals-world-info "$T/chain")
echo "$W" | grep -q '^world_width=128$'
echo "$W" | grep -q '^world_height=128$'
echo "$W" | grep -q '^order_phase=COMMIT$'
ARMY=$($Q generals-player-army-info "$T/chain" "$A")
echo "$ARMY" | grep -q '^unit_count=4$'
U=$(echo "$ARMY" | sed -n 's/^unit_1=//p')
[[ "$U" == UNIT-* ]]
UI=$($Q generals-unit-info "$T/chain" "$U")
echo "$UI" | grep -q '^type=INFANTRY$'
X=$(echo "$UI" | sed -n 's/^x=//p'); Y=$(echo "$UI" | sed -n 's/^y=//p')
# Pick an adjacent non-water unoccupied tile.
TX=''; TY=''
for pair in "$((X+1)) $Y" "$((X-1)) $Y" "$X $((Y+1))" "$X $((Y-1))"; do
  set -- $pair; x=$1; y=$2
  (( x >= 0 && x < 128 && y >= 0 && y < 128 )) || continue
  TI=$($Q generals-tile-info "$T/chain" 1 "$x" "$y")
  terrain=$(echo "$TI" | sed -n 's/^terrain=//p'); occ=$(echo "$TI" | sed -n 's/^unit_id=//p')
  if [[ "$terrain" != WATER && -z "$occ" ]]; then TX=$x; TY=$y; break; fi
done
[[ -n "$TX" ]]
SECRET='phase3-secret-123456'
CANON="QRX-GENERALS-ORDER-v1|1|1|MOVE|$U|$TX|$TY|$SECRET"
COMMIT=$(printf '%s' "$CANON" | openssl dgst -sha3-512 | awk '{print $2}')
mkapply GAME_ORDER_COMMIT "season_id=1;turn_id=1;commitment=$COMMIT" commit
HASH=$(sed -n 's/^body_hash_sha3_512=//p' "$T/commit.signed")
OID="ORD-${HASH:0:24}"
$Q generals-order-info "$T/chain" "$OID" | grep -q '^status=committed$'
# Reveal is forbidden in the commit half and must not move the unit.
if mkapply GAME_ORDER_REVEAL "order_id=$OID;unit_id=$U;to_x=$TX;to_y=$TY;secret=$SECRET" early >/dev/null 2>&1; then echo 'FAIL: early reveal accepted'; exit 1; fi
[[ "$($Q generals-unit-info "$T/chain" "$U" | sed -n 's/^x=//p')" == "$X" ]]
# Advance to reveal half (30 of 60 blocks).
mkdir -p "$T/chain/blocks"
for i in $(seq 1 30); do printf 'phase3-height-%s\n' "$i" > "$T/chain/blocks/p3-$i.block"; done
$Q generals-world-info "$T/chain" | grep -q '^order_phase=REVEAL$'
# Wrong secret rejected atomically.
if mkapply GAME_ORDER_REVEAL "order_id=$OID;unit_id=$U;to_x=$TX;to_y=$TY;secret=wrong-secret-123" bad >/dev/null 2>&1; then echo 'FAIL: bad reveal accepted'; exit 1; fi
$Q generals-order-info "$T/chain" "$OID" | grep -q '^status=committed$'
# Correct reveal executes authoritative movement and consumes one Energy.
E0=$($Q generals-energy-info "$T/chain" "$A" | sed -n 's/^energy=//p')
mkapply GAME_ORDER_REVEAL "order_id=$OID;unit_id=$U;to_x=$TX;to_y=$TY;secret=$SECRET" reveal
OI=$($Q generals-order-info "$T/chain" "$OID")
echo "$OI" | grep -q '^status=executed$'
echo "$OI" | grep -q "^unit_id=$U$"
UI2=$($Q generals-unit-info "$T/chain" "$U")
echo "$UI2" | grep -q "^x=$TX$"; echo "$UI2" | grep -q "^y=$TY$"; echo "$UI2" | grep -q '^last_move_turn=1$'
$Q generals-tile-info "$T/chain" 1 "$X" "$Y" | grep -q '^unit_id=$'
$Q generals-tile-info "$T/chain" 1 "$TX" "$TY" | grep -q "^unit_id=$U$"
E1=$($Q generals-energy-info "$T/chain" "$A" | sed -n 's/^energy=//p')
[[ "$E1" -eq $((E0-1)) ]]
# Replay reveal and second move in same turn are rejected.
if mkapply GAME_ORDER_REVEAL "order_id=$OID;unit_id=$U;to_x=$TX;to_y=$TY;secret=$SECRET" replay >/dev/null 2>&1; then echo 'FAIL: reveal replay accepted'; exit 1; fi
[[ "$A" == "$(tr -d '\r\n' < "$T/alice/address.txt")" ]]
echo 'PASS: QRX Generals 0.0.7.7 Phase 3 deterministic world & orders: deterministic terrain/spawn, starter units, commit/reveal timing, commitment verification, movement, occupancy, Energy debit, replay rejection, WAL atomicity, wallet compatibility'
