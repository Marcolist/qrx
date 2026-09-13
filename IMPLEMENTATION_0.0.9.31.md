# QRX 0.0.9.31 — Deterministic Verifier Selection, Verification Rewards & Consensus Slashing/Jailing

Status: DONE — Core/consensus foundation (2026-09-10)

## Deterministic verifier selection

PoUC receipt acceptance now creates a chain-authoritative verifier selection record instead of allowing validators to self-select.

Selection properties:
- deterministic stake-weighted selection from QRX bonded-validator state,
- entropy is bound to the finalized-parent chain block hash used by the receipt path plus the receipt commitment,
- owner and compute provider are excluded,
- compute-fraud jailed and paused validators are excluded,
- primary and challenger sets are disjoint,
- SPOT selects 1 primary verifier,
- RANDOM selects 2 primary verifiers,
- REDUNDANT_2_OF_3 selects 3 primary verifiers,
- SPOT/RANDOM additionally reserve up to 3 deterministic challengers,
- primary verification window: 20 blocks,
- challenge window: another 20 blocks.

Selection commitment domain:
`QRX/POUC/VERIFIER-SELECTION/V1`

Selection/randomness hashing uses explicit big-endian integer encoding so commitments do not depend on host endianness. Empty selection slots are encoded deterministically for persistence.

## Verification and challenge micro-rewards

`COMPUTE_ESCROW_LOCK` now accepts an optional, explicitly user-authorized `verification_reward_atoms` budget.

This budget is separate from:
- provider compute price,
- normal QRX protocol/staking rewards,
- FastTrack fee,
- FastTrack development share.

Reward weighting in V1:
- primary verifier weight = 2,
- challenger weight = 1.

The deterministic committee record contains the per-primary and per-challenger reward. A verifier/challenger only receives the reward if it is the selected identity and its canonical verification/challenge transaction is accepted.

Reward payout, verification-budget spend accounting, compute-escrow-pool debit, transaction fee, account balance and normal applytx metadata remain in the authoritative QRXDB/WAL state transition. Unused verification budget remains in the escrow and is returned to the owner on final settlement/refund.

## Consensus fraud slashing and jailing

PoUC fraud evidence can now feed the authoritative staking state rather than remaining only a slash/jail candidate record.

There are deliberately no hidden default penalty economics. When a slash/jail candidate is executed, canonical consensus parameters must exist:
- `consensus:compute:params:fraud_slash_bps`
- `consensus:compute:params:fraud_jail_blocks`

Slashing behavior:
- bounded to 0..10000 basis points,
- applies proportionally to validator self stake and delegated stake,
- individual delegation records and delegated total are reduced consistently,
- integer rounding is reconciled deterministically,
- slashed QUB is moved into `consensus:compute:slashing_pool`, not silently minted or burned,
- the same receipt cannot apply the compute penalty twice.

Jailing behavior:
- stores a deterministic block-height jail expiry,
- an existing longer jail cannot be shortened,
- compute-fraud jailed validators are excluded from verifier selection and validator/proposer/voting eligibility paths that consult the canonical validator state.

The QRX supply invariant now includes both the Compute Escrow Pool and Compute Slashing Pool as protocol-held supply.

## Hardening included in this phase

- overflow-safe weighted stake accumulation,
- overflow-safe basis-point slash calculation,
- verification-reward underflow/overspend guards,
- deterministic committee persistence with empty-slot handling,
- endian-independent selection commitments,
- prior 0.0.9.29/0.0.9.30 PoUC tests updated for mandatory selected-verifier enforcement,
- malformed CTest command wiring audit: no add_test command has accidental extra argv tokens.

## Test

New regression target:
`compute_phase156_verifier_selection_rewards_slashing`

The test covers:
- deterministic repeatable selection,
- owner/provider exclusion,
- primary/challenger disjointness,
- rejection of a bonded but non-selected verifier,
- primary and challenge micro-reward accounting,
- escrow-pool accounting,
- unused verification-budget refund during settlement,
- canonical fraud settlement,
- self + delegated stake slashing,
- slashing-pool credit,
- height-based compute jail,
- duplicate-penalty rejection.

Assert-enabled full registered CTest suite: **89/89 PASS**.

## Scope boundary

0.0.9.31 does not yet implement missed-verification penalties, timeout-driven committee reselection, or a dedicated reorg journal for penalty/reward committee state. The deterministic entropy source is the canonical/finalized-parent block hash used by the receipt pipeline; this is not claimed to be a separate VRF/randomness-beacon protocol.
