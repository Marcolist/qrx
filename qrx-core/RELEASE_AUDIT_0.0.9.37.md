# QRX 0.0.9.37 Release Audit

Date: 2026-09-10
Release: AURA Live Scheduler Admission, Compute Provider Identity Binding, Model Cache Placement & Runtime Dispatch

## Source continuity

The release tree is derived from the canonical QRX 0.0.9.36 full-tree archive. A file-level comparison with generated build directories excluded reports **zero removed 0.0.9.36 files**.

New implementation files:

- `qrx-core/src/compute/qrx_compute_provider_identity.h`
- `qrx-core/src/compute/qrx_compute_provider_identity.c`
- `qrx-core/src/compute/qrx_aura_live_dispatch.h`
- `qrx-core/src/compute/qrx_aura_live_dispatch.c`
- `qrx-core/tests/compute_phase162_aura_live_admission_identity_cache_dispatch.c`

Core integration changes are limited to the expected CMake, `qrx.c`, `qrxd.c`, roadmap and release-document paths.

## Clean build audit

A full `cmake --build build --clean-first -j4` was executed with the configured developer/test tree:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

The build completed at **100%**, including `qrx`, `qrxd`, `qrx-cli`, wallet/QRXDB tools and all registered Phase-162 targets.

The build contains **413 inherited warning diagnostics**, the same warning-count baseline recorded for 0.0.9.36. There are **zero warning diagnostics** referencing the new `qrx_compute_provider_identity`, `qrx_aura_live_dispatch`, or Phase-162 source paths. This audit therefore does not claim a warning-free historical tree.

`QRX_REQUIRE_PQC=OFF` is a container developer/test setting. It is not a Mainnet PQC release assertion. The new compute-provider identity path itself is exercised with a real OpenSSL ML-DSA-65 key in Phase 162.

## Test audit

After the final clean rebuild, the complete CTest suite was run again:

- **95 tests registered**
- **95 passed**
- **0 failed**

Phase 162 passed as `compute_phase162_aura_live_admission_identity_cache_dispatch`.

## Native compute-provider identity audit

0.0.9.37 introduces the consensus transaction `COMPUTE_PROVIDER_BIND_IDENTITY`.

The binding is fail-closed:

- zero-value self transaction (`from == to`);
- provider identity bound to transaction owner;
- key scheme restricted to `ML-DSA-65`;
- PEM key parsed and algorithm checked before admission;
- monotonic sequence supports rotation and rejects replay;
- authoritative key stored at `compute/provider-identity/<provider_id>`;
- mutation staged in the existing QRXDB/WAL transaction;
- the generic PoUC tx undo journal snapshots the identity mutation, so a reorg can restore the preceding key/sequence state.

AURA pod gossip now prefers the native compute-provider key lookup and retains the older storage-provider discovery-key lookup only as backward-compatible fallback.

## Model-registry and cache-placement audit

The live scheduler no longer trusts a signed model profile merely because it arrived through gossip. Before use, `qrx_aura_model_profile_registry_verify()` requires the announcement's `model_registry_commitment` to equal the deterministic commitment of the matching `QrxAiModelRegistry` record. Runtime/MoE and minimum resource requirements may not weaken that record.

Cache placement is deterministic. Eligible live MODEL_CACHE pods are ranked using expert locality, cache hit rate, reliability, latency, free cache and pod ID. A dense single-device model must fit its required model storage on one cache pod; distributed/MoE models may split the storage requirement across eligible cache pods.

A cache entry is scheduler-eligible only after the fetch adapter reports the exact expected model commitment. Model bytes remain outside consensus state.

## Live scheduler admission audit

`qrx_aura_live_scheduler_admit()` binds together:

- current signed/non-expired AURA gossip;
- route decision;
- model-registry commitment;
- compute graph commitment and selected node;
- selected provider/pod set;
- actual resource reservations;
- bounded height lease.

The scheduler requires a serving `QrxResourceProviderRuntime`, COMPUTE/AI enablement, compatible runtime device/adapter, verified model cache and sufficient resources. It reserves real provider-runtime compute threads/network egress and rolls previous reservations back if a later reservation fails.

Admission is committed under `QRX/AURA/LIVE-ADMISSION/V1`.

## Runtime-dispatch and PoUC binding audit

A single selected local pod can execute through the existing `QrxMoeWorkerAdapter`. Remote or multi-pod admission requires an explicit distributed-dispatch callback; there is no fake local success path.

Output bytes are committed under `QRX/AURA/RUNTIME-OUTPUT/V1`. The dispatch result is committed under `QRX/AURA/RUNTIME-DISPATCH-RESULT/V1` and includes the output-content commitment as well as admission/model/runtime/provider/pod/size context.

A successful dispatch can be converted to the existing `QrxPoucReceipt`, with the admission commitment stored as `execution_params_commitment`, cryptographically tying PoUC verification/settlement to the scheduler admission that authorized the work.

## Phase-162 adversarial/integration coverage

Phase 162 verifies:

- real ML-DSA-65 compute-provider identity creation and lookup;
- identity sequence replay rejection;
- native compute identity authenticating AURA pod gossip without storage-provider identity;
- identity key rotation followed by PoUC undo-journal reorg rollback;
- exact model-profile/model-registry commitment binding;
- deterministic model-cache planning and verified-cache admission;
- live serving provider-runtime setup and resource reservation;
- AUTO admission;
- local worker-adapter execution;
- output and dispatch-result commitments;
- valid PoUC receipt construction;
- reservation release;
- fail-closed admission without verified model cache;
- deterministic golden vectors for cache plan, admission, output and dispatch result.

## Deliberate scope boundary

0.0.9.37 completes the **core** identity/admission/cache/dispatch layer, but it is not represented as a finished Internet-scale distributed inference deployment.

Still outside this phase:

- dedicated remote/multi-pod AURA dispatch wire protocol;
- cross-node reservation leases over P2P;
- autonomous QRX Drive model-weight/shard fetching in the daemon;
- periodic anti-entropy convergence and automatic local pod self-advertisement;
- persistent governance/chain distribution of the authoritative model registry beyond scheduler-side commitment enforcement;
- WAN-scale concurrent inference/gossip stress testing.

## Packaging gates

The canonical ZIP is accepted only after:

- zero missing baseline files versus 0.0.9.36;
- no build directories in archive;
- `GUIWALLET/src/index.html` present;
- all 0.0.9.37 identity/dispatch source and Phase-162 test files present;
- root and qrx-core implementation/audit/roadmap copies present;
- final build and 95/95 CTest logs present;
- full-tree SHA-256 manifest verifies;
- `unzip -t` reports no archive errors.

## Next

`0.0.9.38 – AURA Remote Dispatch Wire Protocol, Reservation Leases, QRX Drive Model Fetch & Anti-Entropy Convergence`
