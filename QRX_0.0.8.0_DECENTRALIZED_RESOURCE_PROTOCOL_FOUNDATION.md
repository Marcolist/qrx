> **SUPERSEDED ACTIVATION NOTE (0.0.8.1):** The fixed Mainnet height 657000 / Genesis-embedded Resource activation described in this historical 0.0.8.0 checkpoint is no longer the design. Genesis stays on the final 0.0.7.7 contract. `DRIVE_V1` is a post-Genesis mandatory protocol-9 upgrade whose final activation height will be scheduled later through threshold-signed governance, targeting roughly 30 November 2026. See `QRX_0.0.8.1_POST_GENESIS_MANDATORY_UPGRADE_FILESYSTEM.md`.

# QRX 0.0.8.0 — Decentralized Resource Protocol Foundation

## Schedule committed before Mainnet Genesis

- Mainnet Genesis: **2026-09-15 18:00 CEST / 16:00 UTC**, Unix `1789488000`.
- Resource/Storage Mainnet activation height: **657000**.
- Target time at the 10-second target block interval: **2026-11-30 18:00 CET / 17:00 UTC**, Unix `1796058000`.
- Height is consensus-authoritative. The calendar time is an operational target, so actual wall-clock activation may drift with real block production.
- Alpha/Testnet/Regtest activate Resource/Storage at height `0` for development and adversarial testing.

These parameters are now part of canonical Genesis bytes. Because Mainnet Genesis has not happened yet, this is the correct time to commit them. After final Genesis distribution, changing them would change the Genesis hash / chain identity.

## Implemented

- New `src/resource/qrx_resource.[ch]` core module.
- Generic resource types reserved: STORAGE, COMPUTE, GPU, SPECIALIZED.
- Foundation structs for ResourceProvider, ResourceOffer and ResourceContract.
- Mainnet fail-closed activation guard.
- Canonical Genesis resource/storage feature flags and height fork.
- Filesystem is committed as the reference storage backend.
- SeaweedFS is explicitly optional and is not a consensus dependency.
- Storage does not affect validator block-production probability.
- Storage development share fixed at `50 bps = 0.5%` of economic storage contract value.
- Initial resilience reserve fixed at `200 bps = 2%` for simulation/testnet; it remains subject to pre-Mainnet Storage economic review.
- Provider budget receives the remainder (97.5% under the initial split).
- No repeated development fee is defined for future proof/repair/settlement internals.
- Mandatory redundancy profiles committed as protocol defaults:
  - FAST: 3 full replicas.
  - STANDARD: 10+4 erasure profile.
  - ARCHIVE: 16+4 erasure profile.
- Performance bonus cap foundation: +15% maximum (`1500 bps`) so datacenter performance can be rewarded without making home nodes economically irrelevant.
- Sub-linear integer capacity weighting based on `sqrt(proven_free_MiB)` to make more proven storage economically useful without granting linear network power.
- `qrx resource-info <chain-dir> [height]` diagnostic command.
- `qrx storage-split <contract-atoms>` deterministic economic split diagnostic.
- `qrxd` visible version moved to `0.0.8.0-resource-foundation`.

## Verified activation behavior

Synthetic canonical Mainnet Genesis:

- height `656999`: `resource_protocol_active=false`, `storage_protocol_active=false`
- height `657000`: `resource_protocol_active=true`, `storage_protocol_active=true`

Canonical Mainnet Genesis includes:

- `resource_protocol_version=1`
- `resource_activation_height=657000`
- `resource_activation_target_time=1796058000`
- `resource_protocol_enabled=0`
- `storage_protocol_enabled=0`
- `fork.657000.resource_protocol_enabled=1`
- `fork.657000.storage_protocol_enabled=1`
- `storage_reference_backend=filesystem`
- `storage_seaweedfs_optional=1`
- `storage_consensus_affects_block_production=0`
- `storage_dev_share_bps=50`
- `storage_resilience_reserve_bps=200`
- `storage_standard_data_shards=10`
- `storage_standard_parity_shards=4`

## Validation

- Full CMake build: PASS.
- Existing VELOCITY Phase 4/4B/4C/4D/4E/4F tests: PASS.
- Bitcoin SPV hardening test: PASS.
- New Resource Foundation test: PASS.
- Total CTest: **8/8 PASS**.
- Existing 55-locale I18N quality gate: **PASS**.
- Regtest Genesis contains immediate Resource/Storage activation for testing.
- Storage split test for `1,000,000` atoms:
  - Development: `5,000`
  - Resilience: `20,000`
  - Provider budget: `975,000`

## Not active/implemented yet

This phase deliberately does **not** pretend that QRX Drive is complete. The following are next phases:

- native filesystem blob engine with quotas and atomic storage,
- provider registry and bond state,
- capacity proofs,
- client-side PQ encryption and crypto agility,
- erasure coding and deterministic failure-domain placement,
- PoStor challenges,
- QUIC transfer plane,
- contracts/escrow/epoch settlement,
- autonomous repair,
- QRX Drive GUI, Resource Globe and Opportunity Engine.

## Release invariant

0.0.8 code may be deployed before activation, but Mainnet Resource/Storage state transitions must remain fail-closed before height 657000.
