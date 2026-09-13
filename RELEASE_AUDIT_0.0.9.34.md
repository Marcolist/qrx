# QRX 0.0.9.34 – Release Audit

Status: RELEASE FINISHING COMPLETE – 2026-09-10

## Release identity

Release: `0.0.9.34 – PoUC Upstream State Undo Journal & End-to-End Reorg Atomicity`

Canonical predecessor:

`qrx-core-0.0.9.33-canonical-reorg-reward-journal-liveness-governance.zip`

## Full-tree continuity

0.0.9.34 was built from the canonical 0.0.9.33 full-tree archive.

Continuity comparison after implementation and release documentation:

- predecessor files: 1,073
- removed predecessor files: **0**
- added files: 8
- changed existing files: 8

New implementation/test files:

- `qrx-core/src/compute/qrx_pouc_undo.c`
- `qrx-core/src/compute/qrx_pouc_undo.h`
- `qrx-core/tests/compute_phase159_pouc_upstream_undo_e2e_reorg.c`

Changed implementation/build files:

- `qrx-core/CMakeLists.txt`
- `qrx-core/src/compute/qrx_pouc_reorg.c`
- `qrx-core/src/compute/qrx_pouc_reorg.h`
- `qrx-core/src/qrx.c`
- `qrx-core/src/qrxdb.c`
- `qrx-core/src/qrxdb.h`

The root/core roadmap copies were updated. Implementation notes, release audit copies and the final CTest log are additional release artifacts.

## Consensus/reorg audit

### Outer applytx before-image journal

Every 0.0.9.34+ `COMPUTE_*` PoUC pipeline transaction and `POUC_SETTLEMENT` calls `qrx_pouc_tx_undo_stage()` only after the complete outer QRXDB batch has been staged and before commit.

Therefore the undo record covers the actual authoritative write set of that transaction, including account balances, fee pool, nonces, tx-applied/index/payload records and all PoUC state touched by the pipeline/settlement path.

The undo journal is persisted in the same QRXDB/WAL transaction as the state it protects.

### Same-block ordering

Undo identity includes:

- apply height
- QRXDB batch generation
- txid

Rollback sorts by descending generation, so multiple PoUC transactions at the same block height are reversed in exact commit order rather than arbitrary txid order.

### Atomic canonical rollback

All 0.0.9.34+ PoUC undo entries above the requested canonical height are restored in **one QRXDB/WAL batch**. Undo metadata and snapshots are deleted in that same batch.

The canonical reorg coordinator executes this generic transaction-level rollback first. Specialized settlement/liveness/reward rollback routines remain only as backward-compatible fallbacks for older state that predates the 0.0.9.34 outer transaction journal.

### Logical delete correctness

QRXDB now exposes `qrxdb_delete()` and `qrxdb_batch_delete()` using a reserved WAL-compatible tombstone record.

The audit verifies that tombstoned keys:

- are absent from normal reads,
- are absent from prefix scans,
- are excluded from the canonical live-key Merkle calculation,
- are omitted by compaction.

This is required to restore a pre-transaction state exactly when the reverted transaction originally created a new key.

## Build/test audit

Container developer validation configuration:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

The latter is a container/developer-toolchain setting only and does **not** downgrade QRX's production Mainnet PQ requirement.

Results:

- `qrx` executable target: **PASS**
- final registered CTest suite: **92/92 PASS**
- failures: **0**
- new `compute_phase159_pouc_upstream_undo_e2e_reorg`: **PASS**

The source tree still emits historical warnings from pre-existing code (notably the monolithic `qrx.c` and OpenSSL-deprecated compatibility paths); this audit does not claim a warning-free tree.

## Phase 159 assertions

The regression models:

`ESCROW_LOCK -> ASSIGN -> RECEIPT -> VERIFY -> VERIFY -> CHALLENGE -> POUC_SETTLEMENT`

It validates:

- partial rollback to the receipt height while retaining funding/assignment/receipt state,
- deletion of orphaned verification/challenge/settlement keys,
- `tx:applied` deletion and `qrxdb_chain_is_applied()==false`,
- full rollback of owner balance, nonce, fee pool and compute escrow pool,
- exact restoration of the pre-PoUC QRXDB Merkle root,
- one QRXDB generation for the multi-transaction atomic rollback range,
- idempotent repeated rollback,
- successful `qrxdb_verify()`,
- successful compaction without reintroducing deleted keys or changing the restored Merkle root.

## Scope boundary

0.0.9.34 is the authoritative transaction-level reorg path for 0.0.9.34+ PoUC transactions. It is not presented as a universal undo journal for unrelated QRX subsystems.

A larger multi-block clean-replay/crash equivalence harness remains useful as a later release-gate test, but the previously open PoUC upstream state gap is now closed at the transaction/WAL layer.

## Conclusion

**PASS** for 0.0.9.34 full-tree release packaging, subject to the production PQC toolchain policy.

Next planned implementation phase:

`0.0.9.35 – Adaptive AURA Model Fabric, Edge AI Bootstrap & Pod Capacity Registry`
