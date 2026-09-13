# QRX 0.0.9.29 – PoUC Settlement Transaction & Block Pipeline Integration

Status: DONE — 2026-09-09

## What changed

0.0.9.29 moves PoUC settlement from a standalone durable journal into the authoritative QRX transaction/application path.

### New consensus transaction

`POUC_SETTLEMENT`

Properties:
- Velocity transaction with `amount=0`.
- The `to` address must equal the provider in the PoUC receipt.
- Any valid signed QRX wallet may relay the settlement transaction; the relayer cannot redirect payout/refund destinations because recipients come from the canonical receipt + escrow state.
- Mainnet is fail-closed until the threshold-signed protocol schedule activates feature flag `COMPUTE_POUC_V1` at the applicable height.
- The protocol activation is the canonical attestation that the 0.0.9.27 readiness predicates were accepted for Mainnet activation.

### Canonical settlement inputs

The transaction carries the immutable receipt fields and verifier counts. Consensus does not trust transaction-supplied payout flags.

Consensus derives:
- receipt validity and commitment,
- verification result from verifier/matching-verifier counts,
- challenge status from canonical QRXDB challenge state,
- adversarial verdict from canonical QRXDB adversarial state,
- escrow owner and maximum authorization from the canonical persisted compute escrow,
- chain ID / genesis hash / protocol version from the local canonical chain configuration.

### One authoritative WAL state transition

`applytx` now stages all PoUC effects in the same QRXDB batch as fee debit, nonce, applied marker and transaction index:

- Provider wallet credit: verified actual compute.
- User/owner refund.
- FastTrack development share: existing `FASTTRACK_DEV_SHARE_BPS = 50` (0.5% of FastTrack fee only) to the configured QRX development address.
- Remaining FastTrack network share into the QRX consensus fee/network pool.
- Compute escrow-pool debit.
- Durable PoUC journal transition.
- Durable replay marker.
- Post-transition escrow snapshot.
- Immutable settlement history.
- Slash-candidate evidence when warranted.
- Jail-candidate evidence when warranted.
- Sender/relayer transaction fee.
- Sender nonce.
- `tx:applied`, tx location/payload, and consensus apply index.

There is no successful path in which wallet balances are committed but journal/replay state is not, or vice versa: they share the outer applytx WAL commit.

### PAYOUT / FROZEN / REJECTED

PAYOUT:
- provider receives verified compute amount,
- owner receives unused max-compute authorization,
- optional FastTrack fee is split into the existing 0.5% development share and network share,
- the complete locked escrow amount leaves the compute escrow pool.

FROZEN:
- no wallet payout/refund,
- no compute-escrow-pool debit,
- durable FROZEN journal/replay state remains until canonical challenge state resolves.

REJECTED:
- provider receives no compute payout,
- owner receives the complete locked escrow amount, including any FastTrack authorization that was not earned,
- slash/jail candidate evidence is written atomically when the canonical adversarial verdict requires it,
- this module records evidence/candidates; it does not unilaterally destroy validator/provider stake.

### Durable journal refactor

`qrx_pouc_journal_stage(...)` can now stage into an existing QRXDB batch instead of always committing its own batch. The original `qrx_pouc_journal_apply(...)` API remains and wraps the same staging logic for standalone use.

Canonical compute escrow integration helpers were added so the upstream compute-market funding/assignment transaction can persist the escrow snapshot consumed by settlement.

### VELOCITY / block scheduling

`POUC_SETTLEMENT` is accepted by the Velocity transaction schema. It remains a conservative serial/barrier transaction in the speculative MVCC scheduler because it touches shared escrow, fee-pool, replay, challenge/adversarial and multiple wallet keys. This prevents unsafe speculative parallel settlement until it receives a dedicated dynamic write-set adapter.

## Test

New target:

`compute_phase154_pouc_block_pipeline`

It verifies:
- funded compute escrow pool,
- deterministic provider/refund/FastTrack split,
- one-batch payout + journal + replay + escrow state,
- terminal double-settlement rejection,
- full user refund on fraud,
- slash/jail candidate evidence persistence,
- canonical challenge-state FROZEN -> PAYOUT transition with the same replay key,
- pool conservation.

Full registered CTest suite after implementation: **87/87 PASS**.

## Remaining boundary

0.0.9.29 completes the settlement/applytx side. The upstream compute-market funding, assignment, verifier/challenge and adversarial-state writers now have deterministic staging primitives but are not yet represented by their own complete public consensus transaction family. That is the next phase, not a second settlement path.
