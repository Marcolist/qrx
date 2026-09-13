#!/usr/bin/env bash
set -euo pipefail
Q=${1:-./qrx-core/build/qrx}
KEYROOT=${2:-./mainnet-governance-keys}
BACKUP=${3:-./mainnet-governance-backup-vault}
OPERATIONAL=${4:-./mainnet-governance-operational-vault}
[[ -x "$Q" ]] || { echo "qrx binary not found: $Q" >&2; exit 1; }
for i in 1 2 3 4 5; do
  [[ -f "$KEYROOT/DEV_GOV_$i/governance.key" && -f "$KEYROOT/DEV_GOV_$i/governance.pub" ]] || {
    echo "missing DEV_GOV_$i under $KEYROOT" >&2; exit 1;
  }
done
[[ ! -e "$BACKUP" ]] || { echo "backup vault already exists: $BACKUP" >&2; exit 1; }
[[ ! -e "$OPERATIONAL" ]] || { echo "operational vault already exists: $OPERATIONAL" >&2; exit 1; }

"$Q" governance-vault-backup-create "$BACKUP" \
  "$KEYROOT/DEV_GOV_1" "$KEYROOT/DEV_GOV_2" "$KEYROOT/DEV_GOV_3" "$KEYROOT/DEV_GOV_4" "$KEYROOT/DEV_GOV_5"
"$Q" governance-vault-init "$OPERATIONAL"
"$Q" governance-vault-restore-online "$BACKUP" DEV_GOV_1 "$OPERATIONAL"
"$Q" governance-vault-restore-online "$BACKUP" DEV_GOV_2 "$OPERATIONAL"
for i in 3 4 5; do
  "$Q" governance-vault-add-offline "$OPERATIONAL" "$KEYROOT/DEV_GOV_$i/governance.pub"
done
"$Q" governance-vault-status "$OPERATIONAL"
cat <<EOF

Governance Vault V1 prepared.

NEXT SECURITY STEPS (manual):
1. Move/copy "$BACKUP" to encrypted removable OFFLINE media and verify it there.
2. Keep a second geographically separate encrypted backup.
3. Keep DEV_GOV_3..5 private material off the operational machine.
4. Do NOT delete the original key ceremony material until both backups have been independently verified.
5. The operational vault intentionally contains only DEV_GOV_1 and DEV_GOV_2 private keys.
EOF
