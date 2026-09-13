#!/usr/bin/env bash
set -euo pipefail
BIN="${1:-$(cd "$(dirname "$0")/.." && pwd)/build/qrx}"
T="${TMPDIR:-/tmp}/qrx-stealth-phase2-$$"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T"

fail(){ echo "FAIL: $*" >&2; exit 1; }
pass(){ echo "PASS: $*"; }

seed_out=$(QRX_PASSPHRASE=test "$BIN" seed-new "$T/alice")
QRX_PASSPHRASE=test "$BIN" seed-new "$T/bob" >/dev/null
phrase=$(printf '%s\n' "$seed_out" | sed -n 's/^recovery_phrase=//p')
[ -n "$phrase" ] || fail "recovery phrase missing"
cp "$T/alice/recovery.qrxseed" "$T/alice.qrxseed"

# Private derivation must fail closed when the wallet is not unlocked.
if "$BIN" stealth-address "$T/alice" >/dev/null 2>&1; then
  fail "stealth private keys were derivable without wallet unlock"
fi
pass "no public-address private-key fallback"

sa=$(QRX_PASSPHRASE=test "$BIN" stealth-address "$T/alice" | sed -n 's/^stealth_address=//p')
sb=$(QRX_PASSPHRASE=test "$BIN" stealth-address "$T/bob" | sed -n 's/^stealth_address=//p')
[ "${#sa}" -eq 135 ] || fail "unexpected stealth address format"
[ "$sa" != "$sb" ] || fail "distinct wallets generated same stealth address"
pass "wallet-private X25519 scan/spend derivation"

# Recovery of the same primary Ed25519 key must reproduce the same stealth address.
QRX_MNEMONIC="$phrase" QRX_PASSPHRASE=newpass "$BIN" wallet-recover "$T/restored" "$T/alice.qrxseed" >/dev/null
sa2=$(QRX_PASSPHRASE=newpass "$BIN" stealth-address "$T/restored" | sed -n 's/^stealth_address=//p')
[ "$sa" = "$sa2" ] || fail "stealth address changed after wallet recovery"
pass "stealth identity recoverable from wallet recovery pair"

# Minimal regtest chain for send/scan tests.
"$BIN" init-chain "$T/chain" >/dev/null
sed -i.bak 's/^network_id=.*/network_id=qrx-regtest-v1/' "$T/chain/chain.meta" 2>/dev/null || sed -i 's/^network_id=.*/network_id=qrx-regtest-v1/' "$T/chain/chain.meta"
printf '\nfaucet_cap_atoms=1000000\n' >> "$T/chain/chain.meta"
a=$(tr -d '\r\n' < "$T/alice/address.txt")
"$BIN" faucet "$T/chain" "$a" 1000 >/dev/null

# Inject a legacy-format record containing both privacy leaks. A subsequent
# save must migrate it to v2 and strip those fields.
printf '# legacy\nlegacytx|legacy-sender|squb1LEGACYSECRETRECIPIENT|qrx1stealthlegacy|1|%064d|deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef|1|claimed|legacy\n' 0 > "$T/chain/stealth_transfers.db"

send_out=$(QRX_PASSPHRASE=test "$BIN" stealth-send "$T/chain" "$T/alice" "$sb" 123 phase2)
one_time=$(printf '%s\n' "$send_out" | sed -n 's/^one_time_address=//p')
[ -n "$one_time" ] || fail "one-time address missing"

db="$T/chain/stealth_transfers.db"
grep -q '^# QRX stealth public metadata v3' "$db" || fail "v3 metadata header missing"
if grep -q 'squb1LEGACYSECRETRECIPIENT\|deadbeefdeadbeef' "$db"; then
  fail "legacy recipient/shared secret survived metadata migration"
fi
if grep -Fq "$sb" "$db"; then
  fail "recipient stealth address persisted in public transfer DB"
fi
awk -F'|' '!/^#/ && NF != 9 { exit 1 }' "$db" || fail "public stealth records contain unexpected fields"
pass "recipient stealth address and raw shared secret are not persisted"

scan_out=$(QRX_PASSPHRASE=test "$BIN" stealth-scan "$T/chain" "$T/bob")
printf '%s\n' "$scan_out" | grep -q 'scan_found=1' || fail "recipient scan did not find payment"
hist_out=$(QRX_PASSPHRASE=test "$BIN" stealth-history "$T/chain" "$T/bob")
printf '%s\n' "$hist_out" | grep -q 'direction=received' || fail "recipient history ownership derivation failed"
pass "recipient discovers transfer using scan private key"

status=$($BIN privacy-feature-status "$T/chain")
printf '%s\n' "$status" | grep -q 'stealth_key_derivation=private-wallet-material-only' || fail "privacy status missing hardened key policy"
printf '%s\n' "$status" | grep -q 'stealth_spend_path=one-time-secp256k1-ownership-proof-atomic-value-and-replay-state' || fail "spend-path caveat missing"
pass "feature status accurately reports hardening/audit boundary"

echo "QRX Privacy Layer Phase 2 Stealth Hardening: PASS"
