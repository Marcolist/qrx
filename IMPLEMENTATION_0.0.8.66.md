# QRX 0.0.8.66 — Mandatory PRIVATE_PQ Transfer Foundation

Status: implemented and regression-tested.

## Security invariant
PRIVATE QRX Drive payloads must not enter the erasure/provider path as plaintext.

Implemented `qrx_drive_private_pq` container pipeline:
- random 256-bit file key per logical file
- AES-256-GCM chunk encryption (1 MiB transfer-container chunks; independent nonce/tag per chunk)
- per-file random salt and AAD binding salt + chunk index + plaintext length
- hybrid X25519MLKEM768 KEM wrapping of the file key
- SHA3-512 streaming digest over the complete authenticated container body
- ML-DSA-65 signature over that digest
- verify-before-decrypt
- tamper rejection and wrong-signing-key rejection
- bounded-memory encryption/decryption
- destination is removed on decryption/authentication failure

The legacy encrypted hierarchical CAS object implementation remains available. This phase adds the transport-oriented PRIVATE_PQ container required before erasure coding/provider distribution.

Test: `storage_phase108_private_pq`.
