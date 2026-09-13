# QRX 0.0.9.33 — Canonical Reorg Hook, Verification Reward Journal & Liveness Parameter Governance

Status: DONE — 2026-09-10

## Purpose

0.0.9.32 already had explicit rollback helpers for PoUC settlement, verifier reselection and consensus penalties, but the global block-ingest/recovery path did not call them automatically, verifier/challenger reward credits did not have their own rollback journal, and the two liveness policy values were canonical state without a governed update path.

0.0.9.33 closes those three gaps.

## Canonical PoUC reorg hook

New module:

- `qrx-core/src/compute/qrx_pouc_reorg.h`
- `qrx-core/src/compute/qrx_pouc_reorg.c`

New canonical helper:

`qrx_pouc_canonical_reorg_hook()`

It coordinates rollback in this order:

1. durable PoUC settlement journal,
2. liveness/reselection state plus penalty journal,
3. verifier/challenger reward journal.

The ordering is deliberate. A later settlement snapshot can contain verification-budget spend that happened earlier. Settlement is therefore restored first; reward rollback runs last so a settlement rollback cannot accidentally re-introduce an orphaned verifier reward spend.

### Automatic chain-ingest hook

`qrxdb_chain_ingest_block_file()` now detects when recovery/synchronization attempts to replace an already indexed canonical height with a different block hash. Before the replacement block is accepted, it calls the PoUC canonical reorg hook with `height - 1` as the common parent.

Normal BFT finalization should not enter this branch. It is a recovery/reorg safety path for an already indexed divergent height.

## Dedicated verification reward journal

Reward journal prefix:

`consensus:compute:reward_journal:`

For every rewarded `COMPUTE_VERIFY` or `COMPUTE_CHALLENGE`, the outer authoritative applytx batch now also stages a reward journal entry in the same QRXDB/WAL commit.

The journal binds:

- apply height,
- txid,
- receipt commitment,
- graph commitment,
- verifier/challenger address,
- role,
- reward atoms,
- compute escrow pool before/after,
- verification reward spend before/after.

On canonical rollback, entries above the common-parent height are reverted in descending-height order. Rollback:

- subtracts the orphaned verifier/challenger reward from the recipient balance,
- restores the same amount to `consensus:compute:escrow_pool`,
- restores the escrow's `verification_reward_spent_atoms`,
- marks the journal record `REVERTED`.

The operation is restart-safe because the journal itself is QRXDB/WAL-backed. Re-running the same rollback is idempotent: already reverted entries are ignored.

The reward journal intentionally records the reward economic effect, not arbitrary unrelated transaction balance changes.

## Liveness parameter governance

The canonical liveness values remain:

- `consensus:compute:params:verifier_miss_threshold`
- `consensus:compute:params:verifier_miss_jail_blocks`

New validated core API:

- `qrx_pouc_liveness_params_validate()`
- `qrx_pouc_liveness_params_get()`
- `qrx_pouc_liveness_params_stage()`

Zero values are rejected; bounded maximums prevent nonsensical consensus values.

The existing QRX threshold-signed governance framework now supports:

`governance-compute-liveness-propose <proposal-file> <activation-height> <miss-threshold> <miss-jail-blocks>`

Governance action:

`COMPUTE_LIVENESS_PARAMS`

`governance-apply` still requires the configured number of unique valid governance-root signatures. Once the proposal activation height has been reached, both liveness parameters and an immutable parameter-history record are written to QRXDB in a single batch.

History prefix:

`consensus:compute:params_history:`

There are no hidden defaults and no unauthenticated direct runtime mutation path added by this phase.

A real 3-of-3 regtest governance integration was executed successfully: proposal -> three independent signatures -> threshold apply -> canonical QRXDB parameter/history verification.

## Additional hardening

The penalty-journal rollback helper now treats "nothing to revert" as a successful no-op. This was required so the global reorg coordinator can safely call all PoUC rollback helpers even when one subsystem has no entries above the rollback height.

Two pre-existing compiler warnings in the touched penalty-journal function were also removed without changing its penalty economics.

## Test coverage

New regression test:

`compute_phase158_canonical_reorg_reward_governance`

It covers:

- validated canonical liveness parameter staging and loading,
- zero-value policy rejection,
- primary verification reward journaling,
- challenge reward journaling,
- QRXDB close/reopen persistence,
- canonical rollback of multiple rewards in descending height order,
- verifier balance restoration,
- compute escrow-pool restoration,
- verification-budget-spend restoration,
- idempotent second rollback,
- no settlement/liveness false-positive rollback when those journals are empty.

Final validation:

- full registered CTest suite: **91/91 PASS**,
- `qrx` executable target: **build PASS**,
- threshold-signed liveness-governance regtest integration: **PASS**.

## Scope boundary

This phase provides the canonical PoUC reorg coordinator and automatically invokes it when block-index recovery replaces an already indexed height with a different hash. QRX's broader non-PoUC state still depends on its own canonical rollback/reindex machinery; this module does not claim to be a universal undo journal for every QRX subsystem.

The dedicated reward journal reverses verifier/challenger reward economics. A complete upstream PoUC transaction undo journal for funding, assignment, receipt, vote aggregate and challenge-state keys is still a separate hardening step.

## Next

**0.0.9.34 — PoUC Upstream State Undo Journal & End-to-End Reorg Atomicity**
