# QRX 0.0.9.30 – Compute Escrow Funding, Assignment & Verification State Transactions

Status: DONE — 2026-09-10

## Goal

Complete the upstream half of the PoUC consensus pipeline so the 0.0.9.29
`POUC_SETTLEMENT` transaction consumes chain-authoritative funding, assignment,
receipt and verifier/challenge state instead of relying on manually seeded
compute state.

## New Velocity consensus transaction family

All five transactions are gated by the same threshold-signed
`COMPUTE_POUC_V1` activation used by PoUC settlement and are conservative
Velocity serial/barrier transactions.

### `COMPUTE_ESCROW_LOCK`

Owner-authorized funding transaction.

- `amount=0`; economic debit is derived from the signed payload.
- `from == to` is required in V1; funds are not transferred to another wallet.
- payload binds:
  - graph commitment,
  - max compute authorization,
  - optional FastTrack authorization,
  - escrow expiry height.
- applytx debits `max_compute_atoms + fasttrack_fee_atoms` from the owner in the
  same QRXDB/WAL batch that:
  - creates the canonical LOCKED escrow snapshot,
  - credits the consensus compute escrow pool,
  - stores the immutable funding record.
- duplicate funding of an existing graph escrow is rejected.

### `COMPUTE_ASSIGN`

Owner-authorized provider assignment.

- requires a canonical LOCKED escrow owned by `from`.
- `to` is the assigned provider.
- binds quote ID, quote price, quote expiry, node ID, runtime, model commitment,
  verification mode and assignment nonce.
- quote price cannot exceed the owner's max compute authorization.
- quote expiry cannot outlive the escrow.
- assignment commitment uses domain-separated SHA3-256:
  `QRX/POUC/ASSIGNMENT/V1`.
- atomically changes escrow LOCKED -> ASSIGNED and stores the canonical
  assignment record.
- a graph cannot be assigned twice in V1.

### `COMPUTE_RECEIPT`

Provider-authenticated receipt publication.

- transaction signer (`from`) must be the assigned provider.
- `to` must be the escrow owner.
- receipt must match the canonical assignment for:
  - graph,
  - provider,
  - node ID,
  - runtime,
  - model commitment,
  - verification mode.
- verified compute cannot exceed the assigned quote price.
- execution height range must start after assignment and complete no later than
  the block applying the receipt.
- receipt uses the existing `QRX/POUC/RECEIPT/V1` commitment.
- one canonical receipt per graph in V1.

### `COMPUTE_VERIFY`

Canonical verifier attestation.

- V1 consensus verifier writers must have positive bonded QRX validator power
  (self stake, delegated validator power or genesis-validator bond).
- provider and escrow owner cannot verify their own receipt.
- one primary verification vote per verifier and receipt.
- verifier submits the independently observed result commitment; match/mismatch
  is derived by consensus rather than trusted as a boolean supplied by the tx.
- verifier target is determined by the receipt verification mode:
  - SPOT -> 1,
  - RANDOM -> 2,
  - REDUNDANT_2_OF_3 -> 3.
- canonical aggregate stores verifier count, matches, mismatches and finalized
  state.
- finalized SPOT/RANDOM results that are accepted enter canonical challenge
  `PENDING` state.
- finalized verification that is not accepted enters `FAILED` challenge state.
- a failed redundant quorum with at least two independent mismatches creates a
  deterministic adversarial result-mismatch verdict. The code deliberately
  searches prior verifier records if the final arriving vote happens to match,
  so vote ordering cannot suppress mismatch evidence.

### `COMPUTE_CHALLENGE`

Separate asynchronous challenge quorum for SPOT/RANDOM verification.

- only allowed for a finalized verifier aggregate with canonical challenge
  `PENDING`.
- challenger must have positive bonded validator power.
- challenger cannot be the provider, owner, or a primary verifier for the same
  receipt.
- one challenge vote per challenger.
- challenger submits an independently observed result commitment.
- V1 quorum is up to 3 distinct challengers and resolves when either PASS or
  FAIL reaches 2 votes.
- two matching challenge votes -> `PASSED`.
- two mismatching challenge votes -> `FAILED` plus deterministic adversarial
  result-mismatch evidence.

## Settlement hardening

For any escrow that has a 0.0.9.30 canonical assignment, `POUC_SETTLEMENT` now
requires all of the following:

- assignment matches escrow owner and receipt provider,
- assignment node/runtime/model/verification mode matches the receipt,
- receipt is the canonical receipt registered for that graph,
- transaction-supplied receipt commitment matches the canonical receipt,
- canonical verifier aggregate is finalized,
- verifier count and matching count supplied by the settlement transaction must
  equal the canonical aggregate,
- challenge and adversarial state continue to come from canonical QRXDB state.

A narrow legacy compatibility path remains for ASSIGNED escrow snapshots created
before 0.0.9.30 that have no assignment record. This does not create a public
post-activation bypass: `COMPUTE_ESCROW_LOCK` creates only LOCKED state and the
only new public transition to ASSIGNED is `COMPUTE_ASSIGN`.

## applytx / block pipeline

`qrx.c` now recognizes all five upstream Compute transaction types.

- feature activation is fail-closed under `COMPUTE_POUC_V1`.
- `COMPUTE_ESCROW_LOCK` contributes its derived escrow funding amount to the
  sender debit before the single authoritative WAL commit.
- all upstream state is staged in the same batch as transaction fee, nonce,
  applied marker, transaction index and state root.
- transaction index kind: `pouc-upstream-consensus`.
- all five transactions remain MVCC barriers until a dedicated bounded dynamic
  write-set adapter exists.

## Tests

New CTest target:

`compute_phase155_pouc_upstream_transactions`

Coverage includes:

- funding -> LOCKED escrow + escrow pool,
- owner/provider assignment -> ASSIGNED escrow,
- canonical provider receipt,
- 2-of-3 bonded verifier aggregation,
- canonical settlement payout consuming the upstream state,
- rejection of settlement-supplied fake verifier counts,
- RANDOM verification -> PENDING challenge -> 2-vote PASS quorum,
- failed 2-of-3 verification -> adversarial evidence -> settlement reject +
  slash/jail candidates,
- vote-order regression where two mismatches arrive before the final matching
  vote,
- rejection of non-bonded/Sybil verifier identities.

Regression result: **88/88 registered CTest tests PASS**.

## Scope boundary

0.0.9.30 makes funding, assignment, receipt, primary verification and challenge
state authoritative consensus transactions. It does not yet implement:

- deterministic chain-randomness selection of exactly which bonded validators
  are entitled/required to verify each receipt,
- verifier/challenger micro-reward settlement,
- missed-verification penalties,
- direct execution of provider slashing/jailing from slash-candidate evidence,
- a dedicated MVCC dynamic write-set adapter for Compute consensus transactions.

Those are the next consensus-hardening step.
