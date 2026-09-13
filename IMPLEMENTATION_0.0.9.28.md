# QRX 0.0.9.28 – PoUC Consensus State Transition & Durable Replay/Settlement Journal

Status: implemented 2026-09-09

## Goal

Make the 0.0.9.27 PoUC Mainnet settlement decision durable across node restarts and safe against replay or canonical-chain rollback.

## Implemented

- New `qrx_pouc_journal` module backed by QRXDB/WAL.
- Atomic persistence of:
  - active settlement journal entry,
  - replay-consumption state,
  - pre-transition escrow snapshot,
  - post-transition escrow snapshot,
  - immutable settlement history record.
- Journal states:
  - `FROZEN`
  - `PAYOUT`
  - `REJECTED`
  - `REVERTED`
- A `FROZEN` decision may advance to a terminal `PAYOUT` or `REJECTED` using the same replay key after challenge finalization.
- A terminal replay key cannot be applied again, even if a caller supplies a fresh in-memory escrow object.
- Journal application re-runs the 0.0.9.27 deterministic settlement decision and requires exact decision equality before writing state.
- Payout escrow transition is calculated on a copy and only published back to the caller after the QRXDB batch commit succeeds.
- QRXDB/WAL crash recovery therefore remains the durable source for the journal and escrow snapshot.
- Reorg helper restores the latest historical state at or below the new canonical height.
  - Example: payout at height 125 with a prior frozen state at height 120, rollback to 122 restores `FROZEN` rather than freeing the replay key.
  - Rollback below the first settlement transition marks the active entry `REVERTED` and makes the replay key available for the new canonical branch.
- Chain ID, genesis hash, protocol version, receipt commitment, decision commitment, graph commitment and settlement nonce remain carried in the journal entry.
- Delimiter/newline unsafe textual identifiers are rejected by the journal serializer.

## Files

- `qrx-core/src/compute/qrx_pouc_journal.h`
- `qrx-core/src/compute/qrx_pouc_journal.c`
- `qrx-core/tests/compute_phase153_pouc_durable_journal.c`
- `qrx-core/CMakeLists.txt`

## Test coverage

`compute_phase153_pouc_durable_journal` verifies:

- frozen settlement persistence,
- QRXDB close/reopen recovery,
- frozen -> payout transition with the same replay key,
- persistent terminal replay rejection,
- persisted settled escrow state,
- rollback from payout to prior frozen state,
- rollback below the first transition to `REVERTED`,
- replay-key reuse only after that canonical rollback,
- successful settlement again on the new branch.

Full registered CTest suite: **86/86 PASS**.

## Scope boundary

This phase makes the PoUC settlement journal and escrow snapshot durable and reorg-aware. It does **not** yet claim that PoUC settlement is wired into the authoritative block transaction/application pipeline, that wallet balances are transferred by this module, or that consensus slashing is executed here. Those remain separate consensus-state responsibilities.
