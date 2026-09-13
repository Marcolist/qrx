#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./build-phase62/qrx}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T=$(mktemp -d /tmp/qrx-generals-p62.XXXXXX); trap 'rm -rf "$T"' EXIT
$Q init-chain "$T/chain" 20 5000 210000000000000 25000000 200000000000 qrx-regtest 1 5152583037 QRX-Generals-P62 >/dev/null
cat >> "$T/chain/chain.meta" <<META
generals_season_length_blocks=20
generals_turn_length_blocks=4
META
for w in alice bob; do QRX_PASSPHRASE=test $Q seed-new "$T/$w" >/dev/null; done
A=$(tr -d '\r\n' < "$T/alice/address.txt"); B=$(tr -d '\r\n' < "$T/bob/address.txt")
pub(){ openssl pkey -pubin -in "$1/ed25519_pub.pem" -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \n'; }
ml(){ base64 -w0 < "$1/mldsa65_pub.pem"; }
AE=$(pub "$T/alice"); AM=$(ml "$T/alice"); BE=$(pub "$T/bob"); BM=$(ml "$T/bob")
apply(){ local who=$1 type=$2 payload=$3 tag=$4; local a e m;if [[ $who == alice ]];then a=$A;e=$AE;m=$AM;else a=$B;e=$BE;m=$BM;fi;$Q create-velocity-raw-tx "$T/chain" "$a" "$a" 0 "$e" "$m" "$type" 12 1000 "$payload" 0 > "$T/$tag.raw";QRX_PASSPHRASE=test $Q signrawtransactionwithwallet "$T/$who" "$T/chain" "$T/$tag.raw" "$T/$tag.signed" >/dev/null;$Q applytx "$T/chain" "$T/$tag.signed" >/dev/null; }
for who in alice bob; do addr=$([[ $who == alice ]]&&echo "$A"||echo "$B");$Q faucet "$T/chain" "$addr" 20000000000 >/dev/null;apply "$who" GAME_JOIN "display_name=$who" "${who}_join";done
apply alice GAME_CLAN_CREATE 'clan_name=Neon Vanguard;clan_tag=NV' clan
CLAN=$($Q generals-player-info "$T/chain" "$A"|sed -n 's/^clan_id=//p')
apply alice GAME_CLAN_INVITE "invitee=$B;expiry_height=500" invite
HASH=$(sed -n 's/^body_hash_sha3_512=//p' "$T/invite.signed"); INV="CINV-${HASH:0:24}"
apply bob GAME_CLAN_JOIN "invite_id=$INV" joinclan
apply alice GAME_CLAN_OFFICER_SET "slot=1;enabled=1;member=$B" officer
CI=$($Q generals-clan-info "$T/chain" "$CLAN")
echo "$CI"|grep -q "^officer:1=$B$"
apply alice GAME_SEASON_JOIN 'season_id=1' sa
apply bob GAME_SEASON_JOIN 'season_id=1' sb
# Officer can issue map directives.
apply bob GAME_CLAN_DIRECTIVE 'season_id=1;directive=ATTACK;x=5;y=5;expiry_height=15' directive
DI=$($Q generals-clan-directive-info "$T/chain" "$CLAN" 1)
echo "$DI"|grep -q '^type=ATTACK$'; echo "$DI"|grep -q '^x=5$'; echo "$DI"|grep -q "^issuer=$B$"
# Leadership may transfer, but the season command share remains bound to the season-join snapshot.
apply alice GAME_CLAN_LEADER_TRANSFER "member=$B" transfer
CI2=$($Q generals-clan-info "$T/chain" "$CLAN"); echo "$CI2"|grep -q "^leader=$B$"
mkdir -p "$T/chain/blocks";for i in $(seq 1 24);do echo p62 > "$T/chain/blocks/$i.block";done
apply bob GAME_SEASON_FINALIZE 'season_id=1' fin
C=$($Q generals-clan-ranking-info "$T/chain" 1)
echo "$C"|grep -q "^clan_1=$CLAN,200,60000000$"
echo "$C"|grep -q "^clan_1_leader=$A$"
echo "$C"|grep -q '^clan_1_command_atoms=3000000$'
RW=$($Q generals-season-rewards-info "$T/chain" 1); TOTAL=$(echo "$RW"|awk -F',' '/^rank_[0-9]+=/{s+=$3}END{printf "%.0f",s}'); [[ $TOTAL -eq 200000000 ]]
GUI="$ROOT/../GUIWALLET/src/generals/index.html"; TAURI="$ROOT/../GUIWALLET/src-tauri/src/main.rs"
for tok in 'Phase 6.2' clanDirective GAME_CLAN_DIRECTIVE GAME_CLAN_OFFICER_SET GAME_CLAN_LEADER_TRANSFER "5% commander" 'Set ATTACK directive';do grep -q "$tok" "$GUI";done
for tok in generals-clan-directive-info clan_directive;do grep -q "$tok" "$TAURI";done
echo 'PASS: Phase 6.2 clan command authority, officer directives, season leader snapshot, 5% commander share and neon visual overhaul wiring'
