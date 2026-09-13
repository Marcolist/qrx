#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build/qrx}
T=$(mktemp -d /tmp/qrx-generals-p2.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P2 >/dev/null
for w in alice bob; do QRX_PASSPHRASE=test $Q seed-new "$T/$w" >/dev/null; done
A=$(tr -d '\r\n' < "$T/alice/address.txt"); B=$(tr -d '\r\n' < "$T/bob/address.txt")
pub(){ openssl pkey -pubin -in "$1/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n'; }
ml(){ base64 -w0 < "$1/mldsa65_pub.pem"; }
AE=$(pub "$T/alice"); AM=$(ml "$T/alice"); BE=$(pub "$T/bob"); BM=$(ml "$T/bob")
$Q faucet "$T/chain" "$A" 10000000000 >/dev/null
$Q faucet "$T/chain" "$B" 3000000000 >/dev/null
mkapply(){ local who=$1 type=$2 amount=$3 payload=$4 tag=$5; local addr ed mlp; if [[ $who == alice ]]; then addr=$A; ed=$AE; mlp=$AM; else addr=$B; ed=$BE; mlp=$BM; fi; $Q create-velocity-raw-tx "$T/chain" "$addr" "$addr" "$amount" "$ed" "$mlp" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw"; QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/$who" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null; $Q verify "$T/chain" "$T/$tag.signed" >/dev/null; $Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }

# Phase 1 identity foundation remains valid.
mkapply alice GAME_JOIN 0 'display_name=Alice General' a_join
mkapply bob GAME_JOIN 0 'display_name=Bob General' b_join
mkapply alice GAME_CLAN_CREATE 0 'clan_name=First Legion;clan_tag=FL' clan
CLAN=$($Q generals-player-info "$T/chain" "$A" | sed -n 's/^clan_id=//p')
[[ "$CLAN" == CLAN-* ]]

# Deterministic invite ID, acceptance and membership state.
mkapply alice GAME_CLAN_INVITE 0 "invitee=$B;expiry_height=500" invite
HASH=$(sed -n 's/^body_hash_sha3_512=//p' "$T/invite.signed")
INV="CINV-${HASH:0:24}"
IINFO=$($Q generals-clan-invite-info "$T/chain" "$INV")
echo "$IINFO" | grep -q '^status=pending$'
echo "$IINFO" | grep -q "^invitee=$B$"
mkapply bob GAME_CLAN_JOIN 0 "invite_id=$INV" clan_join
[[ "$($Q generals-player-info "$T/chain" "$B" | sed -n 's/^clan_id=//p')" == "$CLAN" ]]
$Q generals-clan-invite-info "$T/chain" "$INV" | grep -q '^status=accepted$'
$Q generals-clan-info "$T/chain" "$CLAN" | grep -q '^member_count=2$'

# A consumed invite cannot be reused after leaving. Founder/leader cannot leave.
mkapply bob GAME_CLAN_LEAVE 0 '-' clan_leave
[[ -z "$($Q generals-player-info "$T/chain" "$B" | sed -n 's/^clan_id=//p')" ]]
if mkapply bob GAME_CLAN_JOIN 0 "invite_id=$INV" replay_join >/dev/null 2>&1; then echo 'FAIL: consumed clan invite reused'; exit 1; fi
if mkapply alice GAME_CLAN_LEAVE 0 '-' founder_leave >/dev/null 2>&1; then echo 'FAIL: founder/leader left without leadership transfer'; exit 1; fi

# A fresh invitation lets the member rejoin.
mkapply alice GAME_CLAN_INVITE 0 "invitee=$B;expiry_height=500" invite2
HASH2=$(sed -n 's/^body_hash_sha3_512=//p' "$T/invite2.signed"); INV2="CINV-${HASH2:0:24}"
mkapply bob GAME_CLAN_JOIN 0 "invite_id=$INV2" clan_join2
$Q generals-clan-info "$T/chain" "$CLAN" | grep -q '^member_count=2$'

# Season 1 is derived solely from chain height. Entry is custodied in the on-chain treasury and fully reserved for the season prize pool.
SINFO=$($Q generals-season-info "$T/chain")
echo "$SINFO" | grep -q '^season_id=1$'
echo "$SINFO" | grep -q '^turn_id=1$'
mkapply alice GAME_SEASON_JOIN 0 'season_id=1' season_a
mkapply bob GAME_SEASON_JOIN 0 'season_id=1' season_b
SINFO=$($Q generals-season-info "$T/chain")
echo "$SINFO" | grep -q '^player_count=2$'
echo "$SINFO" | grep -q '^prize_pool_atoms=200000000$'
TINFO=$($Q generals-treasury-info "$T/chain")
echo "$TINFO" | grep -q '^reserved_atoms=200000000$'

# Duplicate season join is rejected atomically and cannot grow the prize pool twice.
if mkapply alice GAME_SEASON_JOIN 0 'season_id=1' season_dup >/dev/null 2>&1; then echo 'FAIL: duplicate season join accepted'; exit 1; fi
[[ "$($Q generals-season-info "$T/chain" | sed -n 's/^prize_pool_atoms=//p')" == 200000000 ]]

# Deterministic energy: spend 10 in current turn, then regain 1 after six blocks.
mkapply bob GAME_ENERGY_SPEND 0 'season_id=1;turn_id=1;energy=10;action=PHASE2_TEST' energy1
[[ "$($Q generals-energy-info "$T/chain" "$B" | sed -n 's/^energy=//p')" == 90 ]]
mkdir -p "$T/chain/blocks"
for i in $(seq 1 6); do printf 'phase2-height-%s\n' "$i" > "$T/chain/blocks/p2-$i.block"; done
EINFO=$($Q generals-energy-info "$T/chain" "$B")
echo "$EINFO" | grep -q '^height=6$'
echo "$EINFO" | grep -q '^energy=91$'
echo "$EINFO" | grep -q '^turn_id=1$'
mkapply bob GAME_ENERGY_SPEND 0 'season_id=1;turn_id=1;energy=1;action=PHASE2_REGEN_TEST' energy2
[[ "$($Q generals-energy-info "$T/chain" "$B" | sed -n 's/^energy=//p')" == 90 ]]

# Wallet addresses and General assets remain unchanged.
[[ "$A" == "$(tr -d '\r\n' < "$T/alice/address.txt")" ]]
[[ "$B" == "$(tr -d '\r\n' < "$T/bob/address.txt")" ]]
GA=$($Q generals-player-info "$T/chain" "$A" | sed -n 's/^general_asset=//p')
GB=$($Q generals-player-info "$T/chain" "$B" | sed -n 's/^general_asset=//p')
[[ "$($Q asset-balance "$T/chain" "$GA" "$A")" == 1 ]]
[[ "$($Q asset-balance "$T/chain" "$GB" "$B")" == 1 ]]
echo 'PASS: QRX Generals 0.0.7.7 Phase 2 serverless consensus: deterministic seasons/turns, regenerating energy, season entry + reserved prize pool, clan invite IDs, join/leave/rejoin, WAL atomicity, and wallet/address compatibility'
