#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./qrx-core/build/qrx}
OUT=${2:-./mainnet-governance-keys}
[[ -x "$Q" ]] || { echo "qrx binary not found: $Q" >&2; exit 1; }
mkdir -p "$OUT"
echo "Generating 5 Ed25519 developer-governance roots (3-of-5)."
echo "Each invocation asks for a >=12 character private-key passphrase unless QRX_GOV_PASSPHRASE is set."
for i in 1 2 3 4 5; do
  d="$OUT/DEV_GOV_$i"; mkdir -p "$d"
  "$Q" governance-keygen "$d" "DEV_GOV_$i"
done
echo
echo "PUBLIC descriptors to feed the Genesis finalizer:"
for i in 1 2 3 4 5; do echo "$OUT/DEV_GOV_$i/governance.pub"; done
echo "Keep governance.key files offline/private. Never copy them into QRX source or Genesis."
echo
echo "Recommended next step: create one OFFLINE five-key backup vault plus a max-two operational vault:"
echo "  scripts/setup-mainnet-governance-vault.sh '$Q' '$OUT' <offline-backup-vault> <operational-vault>"
