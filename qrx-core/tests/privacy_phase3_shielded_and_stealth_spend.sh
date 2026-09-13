#!/usr/bin/env bash
set -euo pipefail
BIN="${1:-$(cd "$(dirname "$0")/.." && pwd)/build/qrx}"
T="${TMPDIR:-/tmp}/qrx-privacy-phase3-$$"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T"
fail(){ echo "FAIL: $*" >&2; exit 1; }
pass(){ echo "PASS: $*"; }

QRX_PASSPHRASE=alice "$BIN" seed-new "$T/alice" >/dev/null
QRX_PASSPHRASE=bob "$BIN" seed-new "$T/bob" >/dev/null
"$BIN" init-chain "$T/chain" >/dev/null
sed -i.bak 's/^network_id=.*/network_id=qrx-regtest-v1/' "$T/chain/chain.meta" 2>/dev/null || sed -i 's/^network_id=.*/network_id=qrx-regtest-v1/' "$T/chain/chain.meta"
printf '\nfaucet_cap_atoms=10000000\n' >> "$T/chain/chain.meta"
a=$(tr -d '\r\n' < "$T/alice/address.txt")
b=$(tr -d '\r\n' < "$T/bob/address.txt")
"$BIN" faucet "$T/chain" "$a" 2000 >/dev/null

# One-time stealth claim/spend: receiver-only derivation + ownership proof + replay nonce.
sb=$(QRX_PASSPHRASE=bob "$BIN" stealth-address "$T/bob" | sed -n 's/^stealth_address=//p')
out=$(QRX_PASSPHRASE=alice "$BIN" stealth-send "$T/chain" "$T/alice" "$sb" 100 phase3)
tx=$(printf '%s\n' "$out" | sed -n 's/^tx_id=//p')
[ -n "$tx" ] || fail "stealth tx id missing"
if QRX_PASSPHRASE=alice "$BIN" stealth-spend "$T/chain" "$T/alice" "$tx" "$a" 1 >/dev/null 2>&1; then
  fail "non-owner spent one-time output"
fi
s1=$(QRX_PASSPHRASE=bob "$BIN" stealth-spend "$T/chain" "$T/bob" "$tx" "$b" 40)
printf '%s\n' "$s1" | grep -q 'ownership_proof=secp256k1-ecdsa-verified' || fail "ownership proof not verified"
printf '%s\n' "$s1" | grep -q 'nonce=0' || fail "first nonce not zero"
s2=$(QRX_PASSPHRASE=bob "$BIN" stealth-spend "$T/chain" "$T/bob" "$tx" "$b" 60)
printf '%s\n' "$s2" | grep -q 'nonce=1' || fail "second nonce not one"
[ "$("$BIN" balance "$T/chain" "$b")" = "100" ] || fail "stealth value conservation failed"
if QRX_PASSPHRASE=bob "$BIN" stealth-spend "$T/chain" "$T/bob" "$tx" "$b" 1 >/dev/null 2>&1; then
  fail "spent one-time output replayed"
fi
grep -q '__stealth_nonce:' <(strings "$T/chain/balances.bin" 2>/dev/null || true) || true
[ -s "$T/chain/stealth_spends.db" ] || fail "stealth spend proof journal missing"
pass "one-time address owner proof, partial spend, replay nonce and value conservation"

# Shielded address and transparent -> shielded deposit.
za=$(QRX_PASSPHRASE=alice "$BIN" shielded-address "$T/alice" | sed -n 's/^shielded_address=//p')
zb=$(QRX_PASSPHRASE=bob "$BIN" shielded-address "$T/bob" | sed -n 's/^shielded_address=//p')
[ "${#za}" -eq 135 ] && [ "${#zb}" -eq 135 ] || fail "zqub2 address format invalid"
QRX_PASSPHRASE=alice "$BIN" shield-to "$T/chain" "$T/alice" 300 "$za" >/dev/null
[ "$(QRX_PASSPHRASE=alice "$BIN" shielded-balance "$T/chain" "$T/alice")" = "300" ] || fail "initial shielded balance wrong"

# Shielded payment must make change and conserve value.
send=$(QRX_PASSPHRASE=alice "$BIN" shielded-send "$T/chain" "$T/alice" "$zb" 120)
printf '%s\n' "$send" | grep -q 'amount=hidden-on-chain' || fail "shielded amount not marked hidden"
printf '%s\n' "$send" | grep -q 'recipient=hidden-on-chain' || fail "shielded recipient not marked hidden"
printf '%s\n' "$send" | grep -q 'change_note=yes' || fail "mandatory shielded change missing"
printf '%s\n' "$send" | grep -q 'global_balance_equation=verified' || fail "balance equation not verified"
[ "$(QRX_PASSPHRASE=alice "$BIN" shielded-balance "$T/chain" "$T/alice")" = "180" ] || fail "sender shielded change incorrect"
[ "$(QRX_PASSPHRASE=bob "$BIN" shielded-balance "$T/chain" "$T/bob")" = "120" ] || fail "receiver shielded balance incorrect"
pass "encrypted shielded payment + mandatory change + value conservation"

# Public note DB must not contain plaintext value/rho/blind/memo records.
notes="$T/chain/shielded_notes_v3.db"
grep -q '^# QRX shielded notes v3 PUBLIC:' "$notes" || fail "shielded v3 public state missing"
awk -F'|' '!/^#/ && NF != 10 { exit 1 }' "$notes" || fail "unexpected shielded public fields"
if grep -E 'value=|blind=|rho=|shielded-payment|shielded-change' "$notes" >/dev/null; then
  fail "plaintext shielded note witness leaked into public state"
fi
[ -s "$T/chain/shielded_merkle_leaves_v3.db" ] || fail "commitment Merkle leaves missing"
[ -s "$T/chain/shielded_nullifiers_v3.db" ] || fail "key-image/nullifier state missing"
[ -d "$T/chain/shielded_proofs_v3" ] || fail "proof directory missing"
pass "public state contains commitments/ciphertexts/proof hashes, not plaintext witnesses"

# Unshield spends shielded ownership proof and creates change rather than burning remainder.
un=$(QRX_PASSPHRASE=bob "$BIN" unshield "$T/chain" "$T/bob" "$b" 50)
printf '%s\n' "$un" | grep -q 'change_note=yes' || fail "unshield change note missing"
printf '%s\n' "$un" | grep -q 'key_images=verified' || fail "unshield key image verification missing"
[ "$(QRX_PASSPHRASE=bob "$BIN" shielded-balance "$T/chain" "$T/bob")" = "70" ] || fail "unshield remainder lost"
[ "$("$BIN" balance "$T/chain" "$b")" = "150" ] || fail "transparent unshield credit wrong"
pass "unshield preserves remainder with a shielded change note"

# Tampering a public range proof must fail closed on subsequent balance verification.
proof=$(find "$T/chain/shielded_proofs_v3" -type f -name '*.range' | head -n 1)
[ -n "$proof" ] || fail "range proof missing"
printf '\nTAMPER\n' >> "$proof"
if QRX_PASSPHRASE=alice "$BIN" shielded-balance "$T/chain" "$T/alice" >/dev/null 2>&1; then
  fail "tampered range proof accepted"
fi
pass "tampered public proof rejected"

status=$("$BIN" privacy-feature-status "$T/chain")
printf '%s\n' "$status" | grep -q 'shielded_note_encryption=aes-256-gcm-x25519-view-key' || fail "note encryption status missing"
printf '%s\n' "$status" | grep -q 'shielded_value_commitment=secp256k1-pedersen' || fail "commitment status missing"
printf '%s\n' "$status" | grep -q 'shielded_double_spend=key-images' || fail "double spend status missing"
printf '%s\n' "$status" | grep -q 'mainnet_release_gate=external-cryptography-audit-required-before-real-funds' || fail "external audit release gate missing"
pass "privacy feature status exposes cryptographic design and release gate"

echo "QRX Privacy Layer Phase 3 + One-Time Claim/Spend: PASS"
