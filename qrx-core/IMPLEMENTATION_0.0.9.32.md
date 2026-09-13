# QRX 0.0.9.32 — Verifier Timeout/Reselection & Reorg-Safe Penalty Journal

Status: DONE — 2026-09-10

## Purpose

0.0.9.31 made PoUC verifier selection deterministic, stake-weighted and economically accountable, but a selected verifier could still simply disappear. 0.0.9.32 closes that liveness gap without letting a timeout silently change consensus or immediately destroy stake.

The phase adds timeout-driven committee repair, canonical miss accounting and a rollback-safe penalty journal. It deliberately treats missed verification as a liveness failure rather than proof of fraud.

## `COMPUTE_RESELECT`

A new Velocity/PoUC consensus transaction, `COMPUTE_RESELECT`, is accepted by the same authoritative transaction/applytx path as the other PoUC upstream transactions and remains behind the existing fail-closed `COMPUTE_POUC_V1` activation gate.

The transaction carries a receipt commitment and asks consensus to repair the expired verifier committee. It does not choose its own replacement validator.

Rules:

- primary verifiers remain eligible to vote through `verification_deadline_height`; reselection requires `height > deadline`,
- challengers remain eligible through `challenge_deadline_height`; challenger reselection likewise requires strict expiry,
- only committee members that have not written a canonical vote are considered timed out,
- committee members that already voted remain in their slots,
- replacement candidates exclude the job owner, provider, current committee, previous timeout exclusions and compute-jailed validators,
- each timed-out identity is added to the receipt-local exclusion history so it cannot immediately cycle back into a later round.

## Deterministic reselection

Replacement selection is stake-weighted from the canonical bonded validator set and uses fresh finalized-parent block-hash entropy at `apply_height - 1`.

Randomness domain:

`QRX/POUC/VERIFIER-RESELECTION/RANDOM/V1`

The hash binds:

- finalized-parent block hash,
- receipt commitment,
- previous selection commitment,
- reselection round,
- timeout phase,
- committee slot.

Consensus integer fields are explicitly encoded big-endian before hashing, matching the cross-platform deterministic encoding discipline used by the 0.0.9.31 selection commitments.

Primary reselection opens a new 20-block verification window and moves the challenge deadline accordingly. Challenger reselection opens a new 20-block challenge window without reopening primary verification.

## Miss accounting and timeout policy

Canonical miss counters are stored under:

`consensus:compute:verifier_miss_count:<validator>`

The liveness policy is read from canonical chain state:

- `consensus:compute:params:verifier_miss_threshold`
- `consensus:compute:params:verifier_miss_jail_blocks`

There are no hidden defaults. If the policy is absent or zero, reselection fails closed rather than inventing consensus economics.

When a validator reaches the configured miss threshold, 0.0.9.32 applies a **temporary compute jail only**. The timeout path passes `slash_bps=0` into the common consensus penalty engine. Missing a deadline is therefore not treated as cryptographic proof of fraud. Proven fraud from the 0.0.9.31/0.0.9.26 evidence path still uses the separately configured fraud slash + jail rules.

Miss-penalty IDs are domain separated by:

`QRX/POUC/VERIFIER-MISS-PENALTY/V1`

## Reorg-safe penalty journal

The consensus penalty engine now snapshots every state key it changes before a penalty is committed, including as applicable:

- self stake,
- delegated total,
- individual delegation balances,
- compute slashing pool,
- compute jail height,
- duplicate-penalty marker.

Penalty records live under:

`consensus:compute:penalty_journal:`

`qrx_pouc_penalty_journal_revert_above_height()` restores penalties above a canonical rollback height in descending-height order. A duplicate marker that did not exist before the orphaned penalty is restored as `REVERTED`, which explicitly permits the same deterministic penalty to be applied again if it appears on the new canonical branch.

This journal is used by both timeout-jail penalties and the existing fraud slash/jail engine, so fraud stake reductions and slashing-pool credits can also be restored by the reorg helper.

## Reorg-safe liveness history

Each reselection round stores:

- previous verifier selection,
- previous exclusion count,
- timed-out identities,
- miss count before and after,
- any penalty ID produced by the round,
- apply height and phase.

`qrx_pouc_liveness_revert_above_height()` replays orphaned reselection history backwards in descending height order. This is intentional: when several reselection rounds disappear at once, each rollback step sees the state restored by the later rollback step. After liveness state is restored, the penalty journal is reverted above the same canonical height.

## Test coverage

New regression test:

`compute_phase157_verifier_timeout_reorg_penalty`

It covers:

- RANDOM primary committee with one vote and one timeout,
- deterministic replacement of exactly the missing primary,
- exclusion of the timed-out validator,
- miss counter and configured timeout jail,
- rollback restoring the original committee, miss count and jail state,
- deterministic canonical reselection after rollback,
- replacement verifier completing primary verification,
- challenge committee timeout with two silent challengers,
- replacement challengers completing PASS quorum,
- configured fraud stake slash + slashing-pool credit + jail,
- rollback restoring stake, slashing pool and jail,
- canonical fraud penalty re-application after rollback.

Final release validation:

- focused compute phases 134–157: **24/24 PASS**,
- complete registered CTest suite: **90/90 PASS**,
- `qrx` executable target: **build PASS** in the assert-enabled build tree.

## Scope boundary

0.0.9.32 provides the deterministic reorg helpers, but it does not yet claim that every global block-disconnect path automatically invokes them. Verifier/challenger reward credits still use normal transaction-state rollback rather than a dedicated verification-reward journal. The two liveness policy values are canonical state, but a dedicated governed parameter-update transaction is not part of this phase.

Next: **0.0.9.33 — Canonical Reorg Hook, Verification Reward Journal & Liveness Parameter Governance**.
