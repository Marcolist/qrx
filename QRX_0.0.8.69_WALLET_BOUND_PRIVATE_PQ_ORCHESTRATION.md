# QRX 0.0.8.69 — Wallet-bound PRIVATE_PQ Orchestration

Status: implemented.

## Durable wallet recipient identity

PRIVATE QRX Drive data now uses a durable wallet-bound ML-KEM-768 recipient key pair:

- `drive_mlkem768_priv.pem`
- `drive_mlkem768_pub.pem`

The private ML-KEM key is encrypted at rest with a password deterministically derived from the already-encrypted canonical Ed25519 wallet private key. This means wallet passphrase changes do not create a second independent secret that can silently desynchronize from the wallet.

The key is also embedded in `recovery.qrxseed` through `qrx-drive-pq-recovery-v1`, encrypted under a key derived from the canonical Ed25519 private key. New wallets create this recovery extension automatically; `wallet-recovery-refresh` refreshes it; `wallet-recover` restores the same ML-KEM identity.

Important OpenSSL distinction: `X25519MLKEM768` is available as the TLS hybrid group, but this OpenSSL provider does not serialize that group as a durable PKCS#8 wallet key. Therefore persistent at-rest file-key encapsulation is ML-KEM-768 in this phase. `X25519MLKEM768` remains appropriate for transport/TLS hybrid negotiation. The implementation no longer labels the durable file key as the TLS group.

## Daemon / CLI

Added:

- `getdrivepqstatus`
- `preparedriveupload <source> <STANDARD|FAST|ARCHIVE>`
- `startpreparedriveupload <contract_id> <prepare_id>`
- `decryptdrivefile <encrypted-container> <destination>`

`preparedriveupload` loads the wallet ML-KEM public key and existing ML-DSA-65 signing key, creates the encrypted PRIVATE_PQ container, erasure shards, fixed CAS IDs and signed manifest.

`startpreparedriveupload` reloads and re-verifies the prepared package and refuses the transfer unless authoritative contract assignments match the prepared shard CAS IDs exactly.

`decryptdrivefile` requires the wallet ML-KEM private key and ML-DSA public key. Signature/KEM/GCM verification is fail-closed.

## Wallet GUI

The QRX Drive file picker now performs the PRIVATE_PQ preflight immediately. The prepared package ID is retained client-side for the subsequent contract-bound upload. The normal Drive UX no longer sends the selected plaintext source through the old direct erasure-upload path.

## Validation

- full CTest regression: 43/43 PASS
- wallet creation creates the ML-KEM key and recovery extension
- wallet restore recreates the identical ML-KEM public identity
- wallet inline JavaScript passes `node --check`
- Cargo/Tauri check was not available in this build container

## Next

0.0.8.70 — Automatic Storage Contract + Assignment Transaction Orchestration.
