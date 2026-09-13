# QRX 0.0.8.70 — Automatic Storage Contract + Assignment Transaction Orchestration

Status: implemented / Core regression green

This phase closes the manual gap between a PRIVATE_PQ prepared package and consensus storage assignments.

## Implemented

- `qrx_drive_contract` Core planner.
- Deterministic contract ID derived from wallet owner + prepare ID + signed manifest hash.
- Contract quote sized for the selected epochs, all prepared shards and maximum performance factor, including protocol split headroom.
- `STORAGE_CONTRACT_CREATE` payload is generated directly from the already-fixed PRIVATE_PQ package.
- Contract `logical_bytes` is the encrypted container size, so consensus shard sizing exactly matches the prepared erasure shards.
- Exact prepared manifest hash is committed as `manifest_root_hex`.
- Prepared packages now contain true PoStor Merkle roots and leaf counts per shard in addition to CAS IDs and manifest shard roots.
- `STORAGE_ASSIGN` payloads use the exact prepared shard CAS ID, PoStor Merkle root and leaf count.
- Assignment provider selection is now craftable before inclusion: randomness is bound to `inclusion_height - 1` (the last finalized block), not the unknowable hash of the block currently being built.
- Public `qrx_storage_assignment_provider_for_height()` gives wallet/daemon code the same consensus selection result.
- Daemon RPC/CLI `advancepreparedriveupload <prepare_id> [epochs] [rate_atoms_per_gib_epoch]` advances one authoritative step at a time.
- Wallet signs and submits each storage transaction using the existing Ed25519 + ML-DSA transaction signature path.
- `orchestration.state` prevents repeated submission of the same unconfirmed step and remembers a started transfer across restarts.
- Wallet Drive UI now exposes `Create contract & upload` and polls the authoritative orchestration state.
- Provider upload staging can write PENDING/REPAIRING assignments after exact contract/provider/object-ID authorization; ordinary reads remain ACTIVE-only.
- Upload discovery has a separate pending-aware resolver, so download security was not weakened.

## Consensus order

Prepared PRIVATE_PQ package
→ `STORAGE_CONTRACT_CREATE`
→ wait for authoritative contract
→ `STORAGE_ASSIGN` shard 0
→ wait for confirmation
→ recompute deterministic provider with current exclusions
→ ...
→ `STORAGE_ASSIGN` final shard
→ verified provider discovery
→ staged provider upload of the exact prepared shard files

No provider endpoint supplied by the wallet can override consensus provider selection.

## Important remaining lifecycle step

Provider-side `STORAGE_ASSIGN_ACCEPT` is still a separate consensus action. Providers can now stage PENDING shard bytes, but after CAS verification they must automatically sign/submit their own acceptance transaction before the assignment becomes ACTIVE and downloadable/provable.

That is the next step: **0.0.8.71 — Provider Assignment Acceptance + Automatic Post-Upload Activation**.

## Validation

- New regression: `storage_phase111_contract_orchestration`
- Full Core CTest: **44/44 PASS**
- GUI inline JavaScript syntax: PASS
- Cargo/Tauri compile check not run in this environment because `cargo` is unavailable.
