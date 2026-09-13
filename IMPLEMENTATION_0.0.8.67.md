# QRX 0.0.8.67 — PRIVATE_PQ Contract Preflight / Prepared Upload

Status: core preflight implemented and regression-tested.

A PRIVATE_PQ upload must be prepared before consensus assignments are created because encryption is randomized and therefore determines the final shard CAS IDs.

Implemented `qrx_drive_prepare_private_upload()`:
1. plaintext source
2. qrx-drive-pq-v1 encrypted transport container
3. erasure coding (STANDARD 10+4 or supplied profile parameters)
4. stable shard files
5. SHA3-256 CAS object ID per shard
6. SHA3-512 shard roots
7. qrx-drive-pq-v1 manifest
8. ML-DSA-65 manifest signature
9. manifest hash for contract commitment

The prepared package fixes the exact bytes and object IDs that consensus `STORAGE_ASSIGN` records must reference. This removes the unsafe possibility of encrypting again after a contract has already committed to different shard roots.

Next wiring block:
- daemon/CLI `preparedriveupload`
- wallet-bound KEM key loading/backup
- create contract from prepared manifest hash and shard IDs
- bind prepared package to finalized contract
- upload only those exact prepared shards
- download reconstructed PRIVATE_PQ container and decrypt only after signature/KEM/GCM verification
- crash-safe garbage collection of abandoned prepared packages

Test: `storage_phase109_prepare`.
