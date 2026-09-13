# QRX 0.0.9.37 – AURA Live Scheduler Admission, Compute Provider Identity Binding, Model Cache Placement & Runtime Dispatch

Date: 2026-09-10
Status: Implemented and release-validated in developer/test mode

## Goal

Close the execution gap left by 0.0.9.36: authenticated compute-only providers can now bind a native post-quantum discovery identity, signed live AURA capacity can be admitted against actual provider runtime reservations, registered model metadata can drive deterministic cache placement, and admitted work can enter a concrete local worker adapter or an explicit distributed-dispatch transport callback.

## Native compute-provider identity

New files:
- `qrx-core/src/compute/qrx_compute_provider_identity.h`
- `qrx-core/src/compute/qrx_compute_provider_identity.c`

New consensus transaction:
- `COMPUTE_PROVIDER_BIND_IDENTITY`

Properties:
- amount must be zero and `from == to`;
- provider identity is therefore bound to the transaction owner address rather than an arbitrary third-party ID;
- discovery key scheme is `ML-DSA-65`;
- public key is parsed and algorithm-checked before state admission;
- monotonically increasing sequence supports key rotation and rejects replay;
- authoritative state key is `compute/provider-identity/<provider_id>`;
- identity mutation is staged in the same QRXDB/WAL transaction as nonce, fee and tx index;
- the existing generic PoUC transaction undo journal snapshots the identity change, making rotations reorg-revertible;
- AURA pod gossip now resolves a native compute-provider key first and falls back to the older storage-provider discovery key for backward compatibility.

A compute-only provider therefore no longer needs a storage-provider registration merely to authenticate AURA pod gossip.

## Model registry binding and cache placement

New files:
- `qrx-core/src/compute/qrx_aura_live_dispatch.h`
- `qrx-core/src/compute/qrx_aura_live_dispatch.c`

`qrx_aura_model_profile_registry_verify()` binds a signed AURA model profile to the existing deterministic `QrxAiModelRegistry` record. The signed `model_registry_commitment` must exactly match `qrx_ai_model_commitment()` and runtime/MoE/minimum resource requirements may not weaken the registry record.

Cache placement:
- only currently live signed pods with `MODEL_CACHE` capability are candidates;
- candidates are deterministically ordered by expert locality, cache hit rate, reliability, latency, free cache and pod ID;
- dense single-device models must fit their minimum model storage on one pod;
- distributed/MoE profiles may split the required model-storage budget across multiple cache pods;
- placement plans are committed under `QRX/AURA/MODEL-CACHE-PLAN/V1`;
- execution uses a fetch callback so QRX Drive/model-shard transport can plug in without embedding model bytes in consensus;
- a cache entry becomes scheduler-eligible only after the fetch path returns the exact expected model commitment;
- the cache catalog records model/version/pod/verified bytes/height and is used as a hard admission prerequisite.

This follows the existing QRX design rule that model weights remain in QRX Drive while the chain/runtime carries identities, commitments and requirements.

## Live scheduler admission

`qrx_aura_live_scheduler_admit()` performs a fail-closed admission sequence:
1. validate the Compute Job Graph;
2. route against current signed/non-expired AURA gossip;
3. bind the route to the matching model-registry commitment;
4. require the job node's model/version/runtime to match the selected route;
5. require a verified local model-cache entry for every selected execution pod;
6. match each selected pod to a serving `QrxResourceProviderRuntime`;
7. verify compute/AI enablement and runtime-device compatibility;
8. deterministically rank eligible pods;
9. satisfy single-device or distributed memory/throughput/model-cache requirements;
10. reserve compute threads and network egress on the actual provider runtime(s);
11. roll back prior reservations if any later reservation fails;
12. emit a bounded admission lease and canonical commitment.

Admission commitments use `QRX/AURA/LIVE-ADMISSION/V1` and bind route decision, model commitment, graph commitment, node ID, height window and every reserved provider/pod resource tuple.

`qrx_aura_live_scheduler_release()` returns all reserved resources and decrements active jobs.

## Runtime dispatch

`qrx_aura_runtime_dispatch_execute()` validates the live admission commitment again before execution.

Execution paths:
- one local selected pod: dispatches through the existing `QrxMoeWorkerAdapter` after normal worker/device eligibility checks;
- remote or multi-pod admission: requires an explicit distributed-dispatch callback. There is no silent local fallback pretending distributed work happened.

The output bytes are hashed under `QRX/AURA/RUNTIME-OUTPUT/V1`; the execution result then binds that content commitment under `QRX/AURA/RUNTIME-DISPATCH-RESULT/V1` together with admission, model, runtime, primary provider/pod, pod count and output size.

The helper `qrx_aura_dispatch_build_pouc_receipt()` converts the successful dispatch into the existing `QrxPoucReceipt` shape. The admission commitment becomes `execution_params_commitment`, so later PoUC verification and settlement are cryptographically tied to the exact scheduler admission that authorized execution.

## Phase 162 validation

New test:
- `compute_phase162_aura_live_admission_identity_cache_dispatch`

It verifies:
- real OpenSSL ML-DSA-65 key generation and native compute-provider binding;
- provider identity retrieval and key lookup;
- sequence replay rejection;
- ML-DSA-signed AURA pod gossip authenticated by the new compute identity;
- key rotation followed by QRXDB/PoUC undo-journal reorg rollback to the previous identity;
- exact model-profile-to-model-registry commitment binding;
- deterministic cache planning and verified cache admission;
- serving provider runtime setup and actual resource reservation;
- live AURA AUTO admission;
- local worker-adapter execution;
- output-content commitment and dispatch-result commitment;
- conversion to a valid PoUC receipt;
- reservation release;
- fail-closed admission when the verified model cache is absent.

The deterministic cache-plan, admission, output and dispatch-result commitments are pinned by golden vectors in Phase 162. The provider-identity commitment is intentionally not a fixed golden vector because the test generates a fresh ML-DSA key each run.

## Scope boundary

0.0.9.37 delivers the core admission/cache/dispatch and consensus identity primitives, but it does not claim that every distributed inference path is now production-complete. In particular:
- remote multi-pod execution is represented by an explicit transport callback; a dedicated AURA remote-dispatch wire protocol is still required;
- cache placement has a verified fetch adapter boundary, but the daemon does not yet autonomously fetch all model shards from QRX Drive;
- runtime reservations are process-local provider-runtime state, not yet cross-node reservation leases negotiated over P2P;
- automatic local pod self-advertisement and periodic 0.0.9.36 anti-entropy remain pending;
- `QrxAiModelRegistry` commitment checking is now enforced in the scheduler path, but persistent governance/chain distribution of the authoritative registry remains a separate integration concern;
- Phase 162 is deterministic integration testing, not an Internet-scale concurrency or WAN benchmark.

Container validation uses `QRX_BUILD_TESTS=ON` and `QRX_REQUIRE_PQC=OFF` for the wider test tree. The new provider-identity path itself is tested with a real ML-DSA-65 key. This developer build flag is not a Mainnet PQC-release assertion.

## Next

`0.0.9.38 – AURA Remote Dispatch Wire Protocol, Reservation Leases, QRX Drive Model Fetch & Anti-Entropy Convergence`
