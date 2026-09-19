# QRX 0.0.9.75 — Legacy Wallet Password Migration

## Purpose
QRX development builds historically auto-created non-mainnet wallets with a fixed development passphrase when `qrxd`/Core was started without explicit wallet credentials. 0.0.9.75 removes that behavior for new wallets and adds a one-click, locally verified migration path for affected existing wallets.

## Security behavior
- New wallet creation never falls back to the historical development credential on any network.
- Daemon startup never auto-unlocks with that credential.
- The GUI detects the legacy condition only by successfully decrypting the canonical Ed25519 private key locally; the credential is not returned to JavaScript or stored in metadata/configuration.
- Migration requires a new passphrase of at least 12 characters in the dedicated legacy flow.
- Before modification, QRX creates and verifies a copy-only wallet backup.
- Private keys are re-encrypted with AES-256-CBC PKCS#8 temporary files and verified before installation.
- After installation, QRX decrypts the new key files and compares their public-key DER fingerprints and canonical address bytes with the pre-migration identity.
- Any failed post-write/identity check restores the original key files.
- The daemon signer session is refreshed only after successful migration.

## User flow
Wallets → Security. If a historical default-passphrase wallet is detected, the GUI shows `Legacy development password detected` and a dedicated migration control. Enter and confirm a new passphrase, then migrate. Keep the verified backup until the wallet has been independently tested.
