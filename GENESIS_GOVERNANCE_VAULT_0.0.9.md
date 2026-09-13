# QRX 0.0.9 Genesis Governance Vault V1

## Purpose

QRX Mainnet governance remains consensus-level **3-of-5**. Phase 180 adds an operator-safety layout so one normal internet-connected machine does not need access to three governance private keys.

## Recommended layout

- **Offline backup vault:** all five encrypted governance private keys, signing disabled by vault policy and backup filenames.
- **Operational vault:** at most two encrypted private keys; the other three roots are public descriptors only.
- **Offline signer:** when a proposal needs the third signature, sign the exact proposal file on a separate/offline system and bring only the `.qrx` signature file back.

The backup vault is for removable/offline storage. Do not leave it permanently mounted on the operational wallet computer.

## Create five roots

Generate the roots separately and use distinct passphrases where practical:

```bash
qrx governance-keygen /offline/DEV_GOV_1 DEV_GOV_1
qrx governance-keygen /offline/DEV_GOV_2 DEV_GOV_2
qrx governance-keygen /offline/DEV_GOV_3 DEV_GOV_3
qrx governance-keygen /offline/DEV_GOV_4 DEV_GOV_4
qrx governance-keygen /offline/DEV_GOV_5 DEV_GOV_5
```

## Create the single five-key offline backup vault

```bash
qrx governance-vault-backup-create \
  /media/offline/qrx-governance-backup \
  /offline/DEV_GOV_1 \
  /offline/DEV_GOV_2 \
  /offline/DEV_GOV_3 \
  /offline/DEV_GOV_4 \
  /offline/DEV_GOV_5
```

Expected properties:

- `vault_type=OFFLINE_BACKUP`
- `roots=5`
- `threshold=3`
- `signing_disabled=true`
- private files are stored as `governance.key.backup`, not `governance.key`

## Create the operational vault

```bash
qrx governance-vault-init ~/.qrx/governance-operational
```

Restore at most two online roots:

```bash
qrx governance-vault-restore-online /media/offline/qrx-governance-backup DEV_GOV_1 ~/.qrx/governance-operational
qrx governance-vault-restore-online /media/offline/qrx-governance-backup DEV_GOV_2 ~/.qrx/governance-operational
```

Add the other three as public descriptors only:

```bash
qrx governance-vault-add-offline ~/.qrx/governance-operational /offline/DEV_GOV_3/governance.pub
qrx governance-vault-add-offline ~/.qrx/governance-operational /offline/DEV_GOV_4/governance.pub
qrx governance-vault-add-offline ~/.qrx/governance-operational /offline/DEV_GOV_5/governance.pub
```

Check:

```bash
qrx governance-vault-status ~/.qrx/governance-operational
```

Expected: `entries=5`, `online_signers=2`.

## Sign a proposal

Operational signatures:

```bash
qrx governance-vault-sign ~/.qrx/governance-operational DEV_GOV_1 proposal.qrx sig-gov1.qrx
qrx governance-vault-sign ~/.qrx/governance-operational DEV_GOV_2 proposal.qrx sig-gov2.qrx
```

Copy `proposal.qrx` to the offline signer, review its hash/content, then sign with a third root there:

```bash
qrx governance-sign /offline/DEV_GOV_3 proposal.qrx sig-gov3.qrx
```

Bring only `sig-gov3.qrx` back to the online machine. Apply using the unchanged threshold verifier:

```bash
qrx governance-apply <chain-dir> proposal.qrx sig-gov1.qrx sig-gov2.qrx sig-gov3.qrx
```

Consensus verifies three **unique valid Genesis governance-root signatures**.

## Security properties

- Operational vault refuses a third online signer.
- Offline backup vault refuses vault signing.
- Backup private material does not use the ordinary signing filename.
- Imported governance descriptors require a safe key ID, valid hex public key and a fingerprint that exactly matches the public key.
- Governance private keys are never written into chain state.
- Normal wallet users do not need a governance-control GUI. Governance operation remains an explicit advanced/offline workflow.

## Important limit

This vault policy protects against accidental concentration and reduces normal online exposure. It cannot defeat a fully compromised host that can steal already-unlocked online keys, nor can software stop an administrator from deliberately copying offline private material onto another machine. Physical separation, strong distinct passphrases and offline/removable storage remain essential.
