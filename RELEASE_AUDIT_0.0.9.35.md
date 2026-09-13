# QRX 0.0.9.35 Release Audit

Date: 2026-09-10
Release: Adaptive AURA Model Fabric, Edge AI Bootstrap & Pod Capacity Registry

## Source continuity

The 0.0.9.35 tree was derived from the canonical 0.0.9.34 full-tree release. File-level continuity comparison reports **0 removed files**.

Implementation additions:

- `qrx-core/src/compute/qrx_aura_model_fabric.c`
- `qrx-core/src/compute/qrx_aura_model_fabric.h`
- `qrx-core/tests/compute_phase160_aura_adaptive_model_fabric.c`

Implementation changes are limited to `qrx-core/CMakeLists.txt` and the root/qrx-core roadmap copies. Release documents, build/test logs, manifest and continuity report are packaging-only additions.

## Clean build audit

A clean rebuild was executed with `QRX_BUILD_TESTS=ON` and `QRX_REQUIRE_PQC=OFF`. The configured target graph reached 100%, including `qrx`, `qrxd`, `qrx-cli`, `qrx-wallet-cli`, QRXDB tools and Phase 160.

The clean build log contains 413 warning diagnostics in historical source paths. No warning diagnostic references `qrx_aura_model_fabric.c` or `compute_phase160_aura_adaptive_model_fabric.c`. This audit therefore does **not** claim a warning-free historical tree.

`QRX_REQUIRE_PQC=OFF` is strictly the container developer/test setting and is not a production/Mainnet PQC validation claim.

## Test audit

After the clean rebuild, the complete registered CTest suite was rerun:

- **93 tests registered**
- **93 passed**
- **0 failed**

Phase 160 passed as `compute_phase160_aura_adaptive_model_fabric`.

## Functional audit

0.0.9.35 adds a benchmark-driven AURA model-fabric layer with NANO/EDGE/LOCAL/CLUSTER bootstrap tiers, K2/K3 network readiness classes, generic calibrated CPU/CUDA/Metal-MLX/other accelerator participation, utility roles for small nodes, model capability profiles, Resource Globe AI aggregation, AUTO/PINNED routing, graceful degradation, deterministic big-endian SHA3-256 route commitments, deterministic tie-breaks, integer-only readiness math, and binding of selected model/runtime requirements into QRX compute jobs.

Named hardware examples in Phase 160 are synthetic capability fixtures. They prove that Pi-5-like and ODROID-N2+-like ARM nodes are not blocked by a product allowlist; they are not real-world tok/s guarantees.

## Known boundary

This release does not yet provide live cross-node pod gossip, live network telemetry ingestion into the Resource Globe, signed decentralized model-profile distribution, or model-weight distribution. Those belong to 0.0.9.36.

## Packaging audit

Final full-tree package statistics:

- regular files: **1093**
- release tree byte size: verified during final packaging (reported with the external release artifact)
- ZIP byte size: verified after final archive creation (reported with the external release artifact)
- full-tree continuity: **0 removed 0.0.9.34 files**
- actual build-directory entries: **0**
- `GUIWALLET/src/index.html`: present
- root and qrx-core 0.0.9.35 implementation/release documents: present
- root and qrx-core roadmap: present
- Phase-160 source/header/test: present
- SHA-256 full-tree manifest: verified
- ZIP integrity: verified with `unzip -t`

The external `.sha256` file is authoritative for the final ZIP digest.
