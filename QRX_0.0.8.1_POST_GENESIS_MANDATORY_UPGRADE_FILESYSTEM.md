# QRX 0.0.8.1 — Post-Genesis Mandatory Upgrade + Native Filesystem Backend

## Correction to 0.0.8.0
The first 0.0.8.0 draft incorrectly committed a fixed Mainnet Resource/Storage activation height into Genesis. This is superseded.

QRX Genesis remains the stable 0.0.7.7 Genesis contract. DRIVE_V1 is introduced after Genesis as a mandatory, activation-height protocol upgrade through the existing threshold-signed `PROTOCOL_UPGRADE` framework.

### Mainnet activation model
- Genesis target: 2026-09-15 16:00 UTC / 18:00 CEST.
- DRIVE_V1 planned operational target: around 2026-11-30 17:00 UTC / 18:00 CET.
- The date is informational only.
- The final activation height is deliberately NOT hardcoded before Genesis.
- The final height will be chosen after observing real Mainnet block cadence.
- DRIVE_V1 requires protocol version 9 and feature flag `DRIVE_V1`.
- Before a threshold-signed schedule exists, Mainnet Resource/Storage remains fail-closed at every height.
- Once scheduled, Resource/Storage becomes active exactly at the scheduled block height.
- Alpha/Testnet/Regtest remain available for immediate development/testing.

The schedule file format remains backward-compatible with 0.0.7.7:

`activation_height|protocol|min_tx|min_privacy|feature_flags|proposal_hash`

This is intentional: old 0.0.7.7 nodes support protocol level 8 and can still parse a future protocol-9 schedule and report that a mandatory update is required.

Do not schedule the Mainnet height yet. Near the activation window the intended governance flow is equivalent to:

`qrx governance-protocol-propose drive-v1.proposal 9 <FINAL_HEIGHT> <MIN_TX> <MIN_PRIVACY> DRIVE_V1`

followed by the existing governance threshold-signature and apply process.

## Genesis preservation
`qrx-core/src/chain_params.c` no longer writes any of these 0.0.8 values into canonical Genesis bytes:
- resource activation height
- resource target time
- resource/storage enabled flags
- storage economics
- redundancy profiles
- optional SeaweedFS marker

Compared with the final 0.0.7.7 source, the Genesis serialization is unchanged; only a source comment documents the post-Genesis policy.

## Native filesystem reference backend
Filesystem is the official 0.0.8 reference backend. SeaweedFS is optional and has no global/network authority.

Implemented:
- configurable storage root
- persisted backend config
- `max_usage_bytes`
- `min_free_space_bytes`
- SHA3-256 content-addressed objects
- two-level hash-prefix directory layout
- atomic temp-write -> fsync/commit -> rename
- crash recovery by temp cleanup and authoritative on-disk reconciliation
- usage accounting persisted atomically
- idempotent content deduplication
- quota reservation before concurrent large writes
- free-filesystem reserve enforcement
- streaming file ingestion/retrieval
- delete and accounting update
- local backend stats

CLI developer/operator commands:
- `storage-fs-init <storage-root> <max-usage-bytes> <min-free-space-bytes>`
- `storage-fs-info <storage-root>`
- `storage-fs-put <storage-root> <source-file>`
- `storage-fs-get <storage-root> <object-id> <destination-file>`
- `storage-fs-delete <storage-root> <object-id>`
- `storage-fs-recover <storage-root>`

This backend stores opaque bytes. Client-side PQ encryption/sharding is layered above it in later 0.0.8 phases.

## Validation
- Core configure/build with `QRX_REQUIRE_PQC=OFF`, tests enabled: PASS.
- Existing VELOCITY Phase 4/4B/4C/4D/4E/4F regressions: PASS.
- Bitcoin SPV hardening regression: PASS.
- Resource mandatory-upgrade gate test: PASS.
- Filesystem backend test: PASS.
- Filesystem CLI store/get/recover/delete round-trip: PASS.
- 55-locale quality gate remains PASS.
