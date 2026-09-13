# QRX 0.0.8.71 — Provider Assignment Acceptance + Automatic Post-Upload Activation

## Status
DONE

## Implemented
- New `qrx_storage_activation` Core module.
- A provider may consider a PENDING assignment acceptance-ready only after verifying locally stored bytes against all authoritative assignment commitments:
  - provider identity
  - PENDING assignment state
  - exact SHA3-256 CAS object ID
  - exact physical byte count
  - exact PoStor leaf count
  - exact PoStor Merkle root using 64 KiB leaves
- `node-run` automatically scans locally ready PENDING assignments for an enabled storage provider.
- The provider itself constructs and dual-signs `STORAGE_ASSIGN_ACCEPT` using its wallet Ed25519 + ML-DSA-65 identity.
- The acceptance transaction is inserted into the normal node mempool; the file owner cannot impersonate provider acceptance.
- Acceptance submissions are journal-throttled for 12 blocks to avoid duplicate spam while confirmation is pending; restart-safe scanning retries after the window if still PENDING.
- The scan runs after provider startup and immediately after a QRXSTOR PUT, so successful staging can trigger acceptance without an external coordinator.
- Consensus remains authoritative: a shard is readable/healthy only after `STORAGE_ASSIGN_ACCEPT` is included and the assignment becomes ACTIVE.
- Owner-side `advancepreparedriveupload` now reports:
  - `WAITING_PROVIDER_ACCEPT`
  - `HEALTHY_WAITING_REDUNDANCY` once data-shard threshold is ACTIVE
  - `ACTIVE_REDUNDANCY_COMPLETE` once all prepared shards are ACTIVE
- No provider endpoint, IP or private topology data is added to GUI-visible status.

## Regression
- Added `storage_phase112_provider_activation`.
- It proves that a locally present CAS object is still NOT acceptance-ready if its PoStor Merkle commitment is altered.
- Full Core CTest: 45/45 PASS.

## Security boundary
Upload success is not storage health. Provider staging only proves receipt of bytes. Consensus ACTIVE state is reached only through a provider-signed acceptance transaction after local CAS/Merkle verification.

## Next step
0.0.8.72 — Automatic PoStor Proof Runtime + Health/Repair Readiness
