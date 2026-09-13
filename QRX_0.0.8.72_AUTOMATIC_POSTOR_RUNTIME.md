# QRX 0.0.8.72 — Automatic PoStor Proof Runtime

This step closes the automatic proof-of-storage execution loop for ACTIVE QRX Drive provider assignments.

## Runtime

`qrx_storage_postor_runtime` derives the next proof from authoritative assignment state. The next epoch is exactly `last_proof_epoch + 1`. Its challenge block is exactly one `QRX_STORAGE_PROOF_WINDOW_BLOCKS` interval after the last successful proof, or after assignment acceptance for the first proof. The challenge hash must already exist in the finalized QRXDB height index.

The provider reads the challenged 64 KiB leaf from its local content-addressed shard. Merkle siblings are recomputed from local CAS bytes. The implementation intentionally avoids allocating an array containing every leaf hash of a large shard: sibling subtrees are recomputed with bounded range reads and O(log n) proof memory.

Before a transaction is produced, the resulting proof is checked locally against the on-chain assignment Merkle root.

## Provider submission

The node automatically dual-signs `STORAGE_POSTOR` with the provider wallet's Ed25519 and ML-DSA-65 keys and submits it through the normal VELOCITY mempool. A persistent per-assignment/per-epoch marker suppresses duplicate resubmission for 12 blocks and survives restart.

## Consensus hardening

Consensus no longer accepts arbitrary older proof epochs or arbitrary older challenge heights. A proof must match the single deterministic next epoch and challenge height. Replaying an already-authoritative PoStor proof therefore fails closed.

## Health

The runtime derives ACTIVE, PROOF_DUE, DEGRADED and REPAIRING states. Once a proof window is missed, the assignment becomes repair-ready; 0.0.8.73 will consume this hook to execute decentralized replacement-provider repair.

## Verification

`storage_phase113_postor_runtime` verifies automatic scheduling, local proof creation, consensus acceptance, stale-proof replay rejection and DEGRADED/repair readiness.

Full PQ-required Core CTest: **46/46 PASS**.
