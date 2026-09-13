# QRX 0.0.9.32 — Release Packaging / Audit

Date: 2026-09-10
Baseline: `qrx-core-0.0.9.31-verifier-selection-rewards-slashing.zip`
Release: `qrx-core-0.0.9.32-verifier-timeout-reselection-reorg-penalty.zip`

## Full-tree continuity audit

Before adding 0.0.9.32 release documentation, the reconstructed/final implementation tree was compared file-by-file with the canonical full-tree 0.0.9.31 archive, excluding only actual build/CMake-generated directories.

Result:
- 0.0.9.31 baseline files: 1056
- 0.0.9.32 implementation-tree files before release docs: 1059
- new implementation/test files: 3
- removed baseline files: 0
- changed baseline files: 6

Added implementation/test files:
- `qrx-core/src/compute/qrx_pouc_liveness.c`
- `qrx-core/src/compute/qrx_pouc_liveness.h`
- `qrx-core/tests/compute_phase157_verifier_timeout_reorg_penalty.c`

Changed implementation files:
- `qrx-core/CMakeLists.txt`
- `qrx-core/src/compute/qrx_pouc_pipeline.c`
- `qrx-core/src/compute/qrx_pouc_pipeline.h`
- `qrx-core/src/compute/qrx_pouc_verifier.c`
- `qrx-core/src/compute/qrx_pouc_verifier.h`
- `qrx-core/src/qrx.c`

No file from the 0.0.9.31 full project tree disappeared.

After adding the implementation notes, roadmap copy, test log and release audit documents, the release tree contains 1064 files before the new 0.0.9.32 SHA-256 manifest. The final packaged tree therefore contains 1065 regular files once that manifest is added.

## Build/test audit

A fresh external build directory was configured from the final 0.0.9.32 release tree with tests enabled and `QRX_REQUIRE_PQC=OFF` for this container/developer validation environment. This does not relax QRX's production PQ/OpenSSL policy.

Validation result:
- complete build: PASS
- `qrx` executable target: PASS
- registered CTest tests: 90
- passed: 90
- failed: 0
- `compute_phase156_verifier_selection_rewards_slashing`: PASS
- `compute_phase157_verifier_timeout_reorg_penalty`: PASS
- total CTest runtime in this release-finishing run: 4.62 seconds
- compiler warning lines in the complete build log: 0

The full CTest output is included as `RELEASE_AUDIT_0.0.9.32_CTEST_90_OF_90_PASS.log`.

## Consensus/liveness audit notes

The final release source was checked for the 0.0.9.32 invariants:
- `COMPUTE_RESELECT` is registered in the authoritative QRX transaction/applytx path,
- timed-out committee members are replaced only after the exact deadline,
- already-cast canonical votes remain valid,
- replacement selection is stake-weighted and binds finalized-parent entropy, receipt, prior committee commitment, round, phase and slot,
- round/phase/slot are encoded explicitly big-endian in reselection and miss-penalty hashing,
- owner/provider/current committee/timed-out/jailed identities are excluded from replacement selection,
- liveness parameter absence/zero fails closed,
- timeout policy applies compute jail only and does not silently slash stake,
- fraud penalties continue through the configured slash/jail path,
- penalty snapshots cover stake/delegation/slashing-pool/jail/duplicate-marker state,
- penalty and liveness history can be reverted above a canonical rollback height,
- multi-round liveness rollback is applied in descending-height order.

## Packaging rules

Canonical 0.0.9.32 packaging is required to remain a full QRX project tree, not a core-only delta.

Release packaging checks:
- full-tree continuity from 0.0.9.31: PASS
- source/test implementation present: PASS
- no build/CMake-generated directory included: required and checked before ZIP creation
- `.git` metadata excluded
- sorted SHA-256 release-tree manifest generated inside the package
- archive integrity tested after ZIP creation
- external SHA-256 for the ZIP generated separately

## Scope boundary

This release does not claim that every global canonical block-disconnect callback invokes the new liveness/penalty rollback helper. Verification reward credits still lack a dedicated reorg journal matching the penalty journal, and liveness parameters are canonical state keys but do not yet have their dedicated governed parameter-update transaction.

Next hardening milestone:
`0.0.9.33 — Canonical Reorg Hook, Verification Reward Journal & Liveness Parameter Governance`.
