#!/usr/bin/env bash
set -euo pipefail
Q="${1:?qrx binary required}"
T="${TMPDIR:-/tmp}/qrx-phase4-$$"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/issuer"
"$Q" init-chain "$T/chain" 20 5000 2100000000000000 25000000 1000000000000 qrx-regtest 1 5152583734 QRX-Privacy-P4-Compat >/dev/null
openssl genpkey -algorithm Ed25519 -out "$T/issuer/priv.pem" >/dev/null 2>&1
openssl pkey -in "$T/issuer/priv.pem" -pubout -out "$T/issuer/pub.pem" >/dev/null 2>&1
QRX_PASSPHRASE='' "$Q" seed-new "$T/wallet" >/dev/null
# Hidden balance must fail closed before a verified credential exists.
if QRX_PASSPHRASE='' "$Q" hidden-balance "$T/chain" "$T/wallet" >/dev/null 2>&1; then echo 'FAIL: hidden-balance accepted unverified wallet' >&2; exit 1; fi
"$Q" privacy-attester-register "$T/chain" CURA "$T/issuer/pub.pem" >"$T/reg"
EXP=$(( $(date +%s) + 86400 ))
"$Q" privacy-credential-issue "$T/chain" "$T/wallet" CURA "$T/issuer/priv.pem" "$EXP" >"$T/issue"
"$Q" privacy-credential-status "$T/chain" "$T/wallet" >"$T/status"
grep -q '^verified=true$' "$T/status"
QRX_PASSPHRASE='' "$Q" hidden-balance "$T/chain" "$T/wallet" >"$T/balance"
grep -q '^0$' "$T/balance"
# Chain state contains no wallet address, subject salt, raw serial, name, DOB, email or KYC payload.
ADDR=$(tr -d '\r\n' < "$T/wallet/address.txt")
! grep -R -F "$ADDR" "$T/chain" >/dev/null
! grep -R -E 'subject_salt|serial=|name=|dob=|email=|passport|identity_document' "$T/chain" >/dev/null
SERHASH=$(sed -n 's/^serial_hash=//p' "$T/issue")
[ ${#SERHASH} -eq 128 ]
"$Q" privacy-credential-revoke "$T/chain" "$SERHASH" >/dev/null
if "$Q" privacy-credential-status "$T/chain" "$T/wallet" >/dev/null 2>&1; then echo 'FAIL: revoked credential still verified' >&2; exit 1; fi
# Tamper local holder binding: signature/subject verification must fail.
cp "$T/wallet/privacy/verified_privacy.cred" "$T/cred.revoked"
# Issue fresh credential, then modify subject commitment.
"$Q" privacy-credential-issue "$T/chain" "$T/wallet" CURA "$T/issuer/priv.pem" "$EXP" >/dev/null
python3 - "$T/wallet/privacy/verified_privacy.cred" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text();
s=s.replace('subject_commitment=', 'subject_commitment=00', 1); p.write_text(s)
PY
if "$Q" privacy-credential-status "$T/chain" "$T/wallet" >/dev/null 2>&1; then echo 'FAIL: tampered credential accepted' >&2; exit 1; fi
echo 'PASS: Privacy Phase 4 verified privacy credential, hidden balance gate, revocation, tamper rejection and no-PII chain state'
