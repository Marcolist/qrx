#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d /tmp/qrx-generals-p31.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P31 >/dev/null

# Pure deterministic sizing tiers.
[[ "$($Q generals-world-plan 100 | sed -n 's/^world_width=//p')" == 128 ]]
[[ "$($Q generals-world-plan 101 | sed -n 's/^world_width=//p')" == 256 ]]
[[ "$($Q generals-world-plan 501 | sed -n 's/^world_width=//p')" == 512 ]]
[[ "$($Q generals-world-plan 2001 | sed -n 's/^world_width=//p')" == 1024 ]]
[[ "$($Q generals-world-plan 8001 | sed -n 's/^world_width=//p')" == 2048 ]]

newwallet(){
  local name=$1
  QRX_PASSPHRASE=test $Q seed-new "$T/$name" >/dev/null
  local addr ed ml
  addr=$(tr -d '\r\n' < "$T/$name/address.txt")
  ed=$(openssl pkey -pubin -in "$T/$name/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
  ml=$(base64 -w0 < "$T/$name/mldsa65_pub.pem")
  printf '%s|%s|%s\n' "$addr" "$ed" "$ml"
}
AINFO=$(newwallet alice); BINFO=$(newwallet bob)
IFS='|' read -r A AE AM <<<"$AINFO"
IFS='|' read -r B BE BM <<<"$BINFO"
$Q faucet "$T/chain" "$A" 5000000000 >/dev/null
$Q faucet "$T/chain" "$B" 5000000000 >/dev/null
mkapply(){
  local who=$1 addr=$2 ed=$3 ml=$4 type=$5 payload=$6 tag=$7
  $Q create-velocity-raw-tx "$T/chain" "$addr" "$addr" 0 "$ed" "$ml" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"
  QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/$who" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null
  $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null
}
mkapply alice "$A" "$AE" "$AM" GAME_JOIN 'display_name=Alice' ajoin
mkapply bob   "$B" "$BE" "$BM" GAME_JOIN 'display_name=Bob' bjoin
mkapply alice "$A" "$AE" "$AM" GAME_SEASON_JOIN 'season_id=1' aseason
mkapply bob   "$B" "$BE" "$BM" GAME_SEASON_JOIN 'season_id=1' bseason

W=$($Q generals-world-info "$T/chain")
echo "$W" | grep -q '^world_width=128$'
echo "$W" | grep -q '^world_height=128$'
echo "$W" | grep -q '^chunk_size=64$'
echo "$W" | grep -q '^chunks_x=2$'
echo "$W" | grep -q '^chunks_y=2$'
echo "$W" | grep -q '^hq_min_distance=16$'
echo "$W" | grep -q '^player_count=2$'

AA=$($Q generals-player-army-info "$T/chain" "$A")
BA=$($Q generals-player-army-info "$T/chain" "$B")
AHQ=$(echo "$AA" | sed -n 's/^hq_unit=//p')
BHQ=$(echo "$BA" | sed -n 's/^hq_unit=//p')
AI=$($Q generals-unit-info "$T/chain" "$AHQ")
BI=$($Q generals-unit-info "$T/chain" "$BHQ")
AX=$(echo "$AI" | sed -n 's/^x=//p'); AY=$(echo "$AI" | sed -n 's/^y=//p')
BX=$(echo "$BI" | sed -n 's/^x=//p'); BY=$(echo "$BI" | sed -n 's/^y=//p')
DX=$(( AX > BX ? AX-BX : BX-AX )); DY=$(( AY > BY ? AY-BY : BY-AY ))
(( DX + DY >= 16 ))

# Chunk coordinates are exposed and occupancy resolves through chunked QRXDB keys.
TI=$($Q generals-tile-info "$T/chain" 1 "$AX" "$AY")
echo "$TI" | grep -q '^chunk_x='
echo "$TI" | grep -q '^chunk_y='
echo "$TI" | grep -q "^unit_id=$AHQ$"

# Starter infantry must be close to its HQ, not scattered across the world.
for idx in 1 2 3; do
  U=$(echo "$AA" | sed -n "s/^unit_${idx}=//p")
  UI=$($Q generals-unit-info "$T/chain" "$U")
  X=$(echo "$UI" | sed -n 's/^x=//p'); Y=$(echo "$UI" | sed -n 's/^y=//p')
  dx=$(( X > AX ? X-AX : AX-X )); dy=$(( Y > AY ? Y-AY : AY-Y ))
  (( dx <= 2 && dy <= 2 ))
done

[[ "$A" == "$(tr -d '\r\n' < "$T/alice/address.txt")" ]]
[[ "$B" == "$(tr -d '\r\n' < "$T/bob/address.txt")" ]]
echo 'PASS: QRX Generals 0.0.7.7 Phase 3.1 adaptive seasonal world: deterministic size tiers, 64x64 chunk occupancy, fair HQ minimum distance, clustered starter armies, monotonic coordinate-compatible world model'
