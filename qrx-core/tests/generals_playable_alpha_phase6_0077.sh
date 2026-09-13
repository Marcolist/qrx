#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build-phase6/qrx}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T=$(mktemp -d /tmp/qrx-generals-p6.XXXXXX); trap 'rm -rf "$T"' EXIT

# Fast deterministic test-only season. Runtime meta overrides do not alter the
# production defaults embedded in genesis/consensus.
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P6 >/dev/null
cat >> "$T/chain/chain.meta" <<META
generals_season_length_blocks=20
generals_turn_length_blocks=4
META

mk_wallet(){
  local name=$1
  QRX_PASSPHRASE=test $Q seed-new "$T/$name" >/dev/null
  local a ed ml
  a=$(tr -d '\r\n' < "$T/$name/address.txt")
  ed=$(openssl pkey -pubin -in "$T/$name/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n')
  ml=$(base64 -w0 < "$T/$name/mldsa65_pub.pem")
  printf '%s|%s|%s\n' "$a" "$ed" "$ml"
}

declare -A W
W[alice]=$(mk_wallet alice)
W[bob]=$(mk_wallet bob)

apply_for(){
  local who=$1 type=$2 amount=$3 payload=$4 tag=$5
  local addr ed ml
  IFS='|' read -r addr ed ml <<<"${W[$who]}"
  $Q create-velocity-raw-tx "$T/chain" "$addr" "$addr" "$amount" "$ed" "$ml" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"
  QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/$who" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null
  $Q verify "$T/chain" "$T/$tag.signed" >/dev/null
  $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null
}

for who in alice bob; do
  IFS='|' read -r addr _ _ <<<"${W[$who]}"
  $Q faucet "$T/chain" "$addr" 3000000000 >/dev/null
  apply_for "$who" GAME_JOIN 0 "display_name=$who" "${who}_join"
  apply_for "$who" GAME_SEASON_JOIN 0 'season_id=1' "${who}_season"
done

A=${W[alice]%%|*}; B=${W[bob]%%|*}
# Player index is required for deterministic, bounded ranking/finalization.
RANK=$($Q generals-ranking-info "$T/chain" 1)
echo "$RANK" | grep -q '^player_count=2$'
echo "$RANK" | grep -q "player_0="
echo "$RANK" | grep -q "player_1="

# Region endpoint drives the real GUI hex renderer.
HQ=$($Q generals-player-army-info "$T/chain" "$A" 1 | sed -n 's/^hq_unit=//p')
HX=$($Q generals-unit-info "$T/chain" "$HQ" | sed -n 's/^x=//p')
HY=$($Q generals-unit-info "$T/chain" "$HQ" | sed -n 's/^y=//p')
$Q generals-region-info "$T/chain" 1 "$HX" "$HY" 3 3 | grep -q '^tile='

# Advance beyond the 20-block test season without a central game server.
mkdir -p "$T/chain/blocks"
for i in $(seq 1 24); do printf 'phase6-height-%02d\n' "$i" > "$T/chain/blocks/p6-$i.block"; done

# Anyone with a valid signed wallet transaction may finalize an ended season.
apply_for alice GAME_SEASON_FINALIZE 0 'season_id=1' finalize
RESULT=$($Q generals-season-result-info "$T/chain" 1)
echo "$RESULT" | grep -q '^status=finalized$'
WINNER=$(echo "$RESULT" | sed -n 's/^winner=//p')
[[ "$WINNER" == "$A" || "$WINNER" == "$B" ]]
# Phase 6.1 distributes the full pool instead of winner-takes-all.
REWARDS=$($Q generals-season-rewards-info "$T/chain" 1)
TOTAL=$(echo "$REWARDS" | awk -F',' '/^rank_[0-9]+=/{s+=$3} END{printf "%.0f",s}')
[[ "$TOTAL" -eq 200000000 ]]
claim_one(){
  local who=$1 addr=$2 tag=$3
  local reward
  reward=$(echo "$REWARDS" | awk -F'[=,]' -v a="$addr" '$2==a{print $4}')
  [[ "${reward:-0}" -gt 0 ]] || return 0
  local before after
  before=$($Q balance "$T/chain" "$addr" | tail -n1 | tr -d '\r')
  apply_for "$who" GAME_REWARD_CLAIM 0 'season_id=1' "$tag"
  after=$($Q balance "$T/chain" "$addr" | tail -n1 | tr -d '\r')
  [[ $((after-before)) -eq "$reward" ]]
}
claim_one alice "$A" claim_alice
claim_one bob "$B" claim_bob
$Q generals-treasury-info "$T/chain" | grep -q '^reserved_atoms=0$'

# Replay/double claim must fail.
if $Q applytx "$T/chain" "$T/claim_alice.signed" >/dev/null 2>&1; then
  echo 'FAIL: reward claim replay accepted' >&2; exit 1
fi

# Phase 6 GUI/core bridge and visual map surface are release requirements.
SRC="$ROOT/src/qrx.c"
TAURI="$ROOT/../GUIWALLET/src-tauri/src/main.rs"
GUI="$ROOT/../GUIWALLET/src/generals/index.html"
for tok in GAME_SEASON_FINALIZE GAME_REWARD_CLAIM generals-region-info generals-ranking-info generals-season-result-info generals-season-rewards-info generals-clan-ranking-info player_index; do grep -q "$tok" "$SRC"; done
for tok in generals_snapshot generals_submit_action generals_order_commitment; do grep -q "$tok" "$TAURI"; done
for tok in '<svg' 'hex' 'GAME_ORDER_COMMIT' 'GAME_ORDER_REVEAL' 'GAME_REWARD_CLAIM' 'grid-aurora-drift.mp3'; do grep -q "$tok" "$GUI"; done

echo 'PASS: Generals Phase 6 playable-alpha lifecycle, deterministic ranking/finalization, atomic QUB reward claim, replay rejection, region API and GUI bridge surface'
