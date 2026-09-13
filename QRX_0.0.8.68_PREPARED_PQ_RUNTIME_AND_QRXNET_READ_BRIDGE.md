# QRX 0.0.8.68 — Prepared PRIVATE_PQ Runtime + QRX-Net Read Bridge

## Implemented

- Prepared PRIVATE_PQ packages now persist `prepare.meta` (`qrx-drive-prepared-v1`).
- Reload revalidates `qrx-drive-pq-v1`, ML-DSA manifest signature, manifest hash, and every persisted shard SHA3-256 CAS ID.
- Runtime adds `qrx_drive_runtime_start_prepared_upload()`.
- Prepared upload never re-encrypts or re-erasure-codes the source.
- Every verified provider source/assignment object ID must match the exact preflight shard CAS ID for that shard index.
- Mismatch fails closed before provider upload starts.
- Existing legacy `startdriveupload` remains for compatibility/testing but MUST NOT be described as the PRIVATE_PQ path.
- Added daemon read RPC `getdomain <name.qrx>` backed by authoritative QRXDB registry state.
- Added daemon/CLI `resolvebrowserinput <input>` backed by the QRX resolver. `.qrx`/`qrx://` routes report `dns_allowed=false`; normal WWW routes remain separate.
- New regression `storage_phase110_prepared_runtime`.
- Full Core test suite: 43/43 PASS.

## Still open before 0.0.8 branch completion

1. Wallet-bound X25519MLKEM768 key persistence and backup/recovery integration.
2. `preparedriveupload` daemon/Tauri workflow that creates preflight from the unlocked wallet and then creates/signs the matching `STORAGE_CONTRACT_CREATE` + deterministic `STORAGE_ASSIGN` transactions.
3. PRIVATE_PQ restore wrapper in daemon/Tauri: reconstruct encrypted container -> verify signature -> KEM unwrap -> bounded AES-GCM decrypt -> atomic destination.
4. Prepared package GC / resumable rehydration across daemon restart.
5. QRX-Net product wiring: domain management RPC/UI, PUBLIC_SIGNED publisher, website fetch/cache, integrated WWW+QRX browser, sandbox and permission bridge UI.
6. QRX-Net Globe/hosting mission integration, adversarial testnet and DRIVE_V1 + QRX_NET_V1 readiness gates.
7. Genuine QUIC backend remains optional/desired; `quic://` must stay fail-closed until a real implementation is linked.

## Branch status

0.0.8 is NOT complete. Core foundations for naming, registry/economics, resolver, PUBLIC_SIGNED verification, DApp permissions and storage already exist, but the end-user/daemon integration and final readiness gates remain.
