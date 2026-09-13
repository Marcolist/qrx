# QRX 0.0.9.36 Release Audit

Date: 2026-09-10
Release: AURA Pod Gossip, Live Resource Globe AI Telemetry & Model Profile Distribution

## Source continuity

The 0.0.9.36 tree is derived from the canonical 0.0.9.35 full-tree release. File-level continuity comparison with build directories excluded reports **0 removed baseline files**.

Implementation additions:

- `qrx-core/src/compute/qrx_aura_fabric_gossip.h`
- `qrx-core/src/compute/qrx_aura_fabric_gossip.c`
- `qrx-core/tests/compute_phase161_aura_pod_gossip_live_globe.c`

Implementation changes:

- `qrx-core/src/compute/qrx_aura_model_fabric.h`
- `qrx-core/src/compute/qrx_aura_model_fabric.c`
- `qrx-core/src/qrx.c`
- `qrx-core/src/qrxd.c`
- `qrx-core/src/qrx_cli.c`
- `qrx-core/CMakeLists.txt`
- root/qrx-core roadmap copies

The final continuity report is stored as `RELEASE_AUDIT_0.0.9.36_CONTINUITY.txt`.

## Clean build audit

A clean rebuild was executed with:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

The configured target graph reached 100%, including `qrx`, `qrxd`, `qrx-cli`, `qrx-wallet-cli`, QRXDB tools, previous storage/network/compute phases, and Phase 161.

The final clean build contains **413 warning diagnostics**, matching the inherited 0.0.9.35 historical warning count. During release finishing, seven new misleading-indentation diagnostics introduced in the 0.0.9.36 gossip/helper code were removed. The final build log contains no warning referencing `qrx_aura_fabric_gossip.c`, `qrx_aura_model_fabric.c`, or `compute_phase161_aura_pod_gossip_live_globe.c`.

This audit therefore does not claim that the historical QRX tree is warning-free.

`QRX_REQUIRE_PQC=OFF` is strictly the container developer/test mode and is not a production/Mainnet PQC validation claim.

## Test audit

After the final clean rebuild, the complete registered CTest suite was rerun:

- **94 tests registered**
- **94 passed**
- **0 failed**

Phase 161 passed as `compute_phase161_aura_pod_gossip_live_globe`.

## Functional audit

0.0.9.36 adds signed, height-bounded AURA pod-capacity and model-profile gossip with canonical big-endian serialization, SHA3-256 domain-separated commitments, signature verification, replay/sequence protection, provider takeover protection, cache persistence with re-verification, expiry pruning, bounded P2P PUSH/PULL handlers, live fabric status, Resource Globe privacy aggregation, and live AUTO-routing input derived from current signed announcements.

The daemon adds `getaurafabric`, `getauraatlas`, and `listauramodels`; `qrx-cli` forwards those methods and the clean build confirms the CLI help contains all three commands.

The global gossip view supports up to 4096 pods and 256 model profiles. The 0.0.9.35 local in-memory scheduler registry remains separately capped at 256 pods.

## Identity and trust boundary

Pod gossip currently authenticates the provider through the existing consensus-bound storage-provider discovery key lookup. This is a real authenticated path, but it means a compute-only provider that lacks that existing binding cannot yet publish independently through the V1 daemon path.

Model-profile announcements are signed by active governance-root identities. The announcement signs/binds `model_registry_commitment`, but this release does not additionally prove that the commitment maps to an authoritative on-chain model-registry object.

## P2P boundary

`AURA_POD_PUSH`, `AURA_MODEL_PUSH`, `AURA_POD_PULL`, `AURA_MODEL_PULL`, and `AURA_FABRIC_STATUS` are wired into the node protocol. Accepted PUSH records are persisted and fanned out to a bounded subset of known peers; invalid gossip incurs peer-reputation penalty.

Phase 161 validates the deterministic protocol/data layer, not a large multi-process network. This release does **not** claim periodic anti-entropy convergence, automatic local pod self-publication, Internet-scale gossip behavior, or a WAN stress benchmark.

## Model distribution boundary

0.0.9.36 distributes signed model capability metadata only. It does **not** distribute model weights/chunks, place model caches, reserve live pod capacity, or dispatch inference execution. Those are the next scheduler/runtime integration layer.

## Packaging acceptance gates

The canonical full-tree ZIP is accepted only if all of the following pass:

- zero removed 0.0.9.35 files;
- no build directories/compiled build tree in the archive;
- `GUIWALLET/src/index.html` present;
- all new 0.0.9.36 source/header/test files present;
- root and qrx-core 0.0.9.36 implementation/release documents present;
- root and qrx-core roadmap present;
- Phase-161 test present;
- clean build log and 94/94 CTest log present;
- SHA-256 full-tree manifest verifies every listed release file;
- `unzip -t` reports no archive error.

## Next

`0.0.9.37 – AURA Live Scheduler Admission, Compute Provider Identity Binding, Model Cache Placement & Runtime Dispatch`
