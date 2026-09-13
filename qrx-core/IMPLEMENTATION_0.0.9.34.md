# QRX 0.0.9.34 – PoUC Upstream State Undo Journal & End-to-End Reorg Atomicity

Status: implemented 2026-09-10

## Goal

Close the remaining PoUC reorg gap after 0.0.9.33. Funding, assignment, receipt, verification/challenge state and the outer applytx accounting must roll back together rather than depending on independent best-effort cleanup helpers.

## Implemented

### 1. Generic PoUC outer-transaction undo journal

New module:

- `qrx-core/src/compute/qrx_pouc_undo.h`
- `qrx-core/src/compute/qrx_pouc_undo.c`

Every 0.0.9.34+ PoUC upstream transaction and `POUC_SETTLEMENT` now stages a pre-state undo record in the **same QRXDB/WAL batch** as the authoritative transaction apply.

The undo layer snapshots every unique key that has already been staged by the outer applytx batch before commit. This deliberately includes more than the compute-specific object itself, e.g.:

- account balance changes
- verifier/challenger reward balance changes
- compute escrow pool changes
- fee-pool changes
- account/Velocity nonce
- `tx:applied`
- transaction location/payload/apply index records
- funding state
- assignment state
- receipt state and graph mapping
- deterministic verifier selection state
- verification votes and aggregate
- challenge votes/aggregate/state
- liveness/reselection state and penalty journals when touched by that transaction
- verification reward journal writes
- PoUC settlement/journal state and settlement accounting when the transaction is `POUC_SETTLEMENT`

Undo snapshots are keyed by `(apply_height, QRXDB generation, txid)`. The QRXDB generation is part of the key so multiple PoUC transactions in the same block can be reversed in the exact opposite order in which they were committed.

Snapshot key/value bytes are hex framed rather than delimiter-framed. The journal fails closed if a pre-existing value exceeds the bounded 1 MiB undo snapshot limit.

### 2. Atomic multi-transaction rollback

`qrx_pouc_tx_undo_revert_above_height()` collects all 0.0.9.34+ PoUC transaction journals above the requested canonical height, sorts them by descending apply generation and restores **all affected keys in one QRXDB/WAL batch**.

The corresponding undo metadata and snapshot records are deleted in that same commit. Therefore:

- partial PoUC rollback is not exposed as a committed state,
- a crash after WAL COMMIT remains recoverable by QRXDB,
- repeated rollback calls are idempotent,
- rollback below several PoUC transactions is one authoritative state transition.

### 3. Real QRXDB delete/tombstone semantics

QRXDB now exposes:

- `qrxdb_delete()`
- `qrxdb_batch_delete()`

Deletion is represented internally by a reserved tombstone record so it remains WAL-compatible. Tombstoned keys are treated as absent by normal reads and prefix scans, are excluded from the canonical Merkle root, and are omitted by compaction.

This is required for exact reorg restoration: a key that did not exist before a reverted transaction must become truly absent from logical state rather than being replaced by an empty or fake value.

### 4. Canonical reorg hook integration

`qrx_pouc_canonical_reorg_hook()` now runs the 0.0.9.34 atomic transaction undo layer first.

After that, the existing settlement, liveness/penalty and verification-reward rollback helpers remain as backward-compatible fallback paths for state created by older releases that did not yet carry the generic outer-transaction undo journal.

The canonical divergent-height recovery/sync hook introduced in 0.0.9.33 therefore gains complete 0.0.9.34+ PoUC transaction rollback coverage without dropping compatibility with historical journal formats.

### 5. CMake/test registration cleanup

The inherited phase157 `add_test()` registration accidentally passed the phase158 executable name as an extra command-line argument. 0.0.9.34 removes that stale argument and explicitly adds phase158 and phase159 to the common compute test target options list.

## Validation

New regression test:

`compute_phase159_pouc_upstream_undo_e2e_reorg`

It creates a sequence representing:

`ESCROW_LOCK -> ASSIGN -> RECEIPT -> VERIFY -> VERIFY -> CHALLENGE -> POUC_SETTLEMENT`

and verifies:

- same-height transaction ordering is preserved by QRXDB generation,
- rollback from height 15 to 11 reverses all later PoUC transactions in one WAL generation,
- funding/assignment/receipt state at height 11 remains intact,
- verification/challenge/settlement keys disappear when their creating transactions are reverted,
- `tx:applied` is logically deleted and `qrxdb_chain_is_applied()` returns false,
- rollback from height 11 to 0 restores account balance, nonce, fee pool and compute escrow pool,
- the canonical QRXDB Merkle root exactly matches the pre-PoUC root,
- undo metadata is itself removed,
- a repeated rollback is a no-op,
- QRXDB verification succeeds after rollback,
- QRXDB compaction preserves the exact restored Merkle root and deleted-key semantics.

Final registered CTest suite: **92/92 PASS**.

`qrx` executable target: **PASS**.

Developer/container validation uses `QRX_REQUIRE_PQC=OFF`; this is not a production policy downgrade and does not alter the Mainnet PQ requirement.

## Scope boundary

0.0.9.34 provides exact transactional rollback for **0.0.9.34+ PoUC applytx transactions** and retains the older specialized journals as compatibility fallbacks. It does not claim that every unrelated QRX subsystem now shares this compute undo journal; storage, markets, privacy, Generals and other consensus modules retain their own state-transition/reorg mechanisms.

The next AI/compute phase is intentionally separate from consensus reorg correctness.
