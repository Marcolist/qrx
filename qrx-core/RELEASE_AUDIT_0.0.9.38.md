# QRX 0.0.9.38 Release Audit

Date: 2026-09-10
Release: AURA Remote Dispatch Wire Protocol, Reservation Leases, QRX Drive Model Fetch & Anti-Entropy Convergence

## Source continuity

The 0.0.9.38 tree is derived from the canonical 0.0.9.37 full-tree release. File-level continuity comparison with generated build directories excluded reports **0 removed baseline files**.

New implementation files:

- `qrx-core/src/compute/qrx_aura_remote_dispatch.h`
- `qrx-core/src/compute/qrx_aura_remote_dispatch.c`
- `qrx-core/tests/compute_phase163_aura_remote_dispatch_leases_drive_anti_entropy.c`

Existing implementation files changed by this phase:

- `qrx-core/src/compute/qrx_aura_fabric_gossip.h`
- `qrx-core/src/compute/qrx_aura_fabric_gossip.c`
- `qrx-core/src/qrx.c`
- `qrx-core/CMakeLists.txt`
- root/qrx-core roadmap copies

The source continuity detail is stored in `RELEASE_AUDIT_0.0.9.38_CONTINUITY.txt`.

## Clean build audit

A clean configured build was completed with:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

The build reached 100%, including the core daemon/CLI targets and the new Phase-163 target.

The final clean build contains **413 compiler warning diagnostics**, which matches the inherited 0.0.9.37 warning count. No warning line in the final release build references `qrx_aura_remote_dispatch`, `compute_phase163`, `qrx_aura_fabric_gossip`, or the new anti-entropy tick helper. This audit therefore does **not** claim a warning-free historical QRX tree; it claims that 0.0.9.38 introduces no additional warning count in the validated configuration.

`QRX_REQUIRE_PQC=OFF` is strictly the container developer/test configuration. It is not a production/Mainnet PQC validation claim and does not relax the intended Mainnet PQ policy.

A sanitizer build was started during development but did not complete within the execution window. It is not counted as a release validation result.

## Test audit

After the final clean build, the complete registered CTest suite was run:

- **96 tests registered**
- **96 passed**
- **0 failed**
- Phase 163 passed as `compute_phase163_aura_remote_dispatch_leases_drive_anti_entropy`

## Remote dispatch protocol audit

0.0.9.38 introduces the authenticated AURA remote frame protocol under:

- `QRX/AURA/REMOTE-FRAME/V1`
- `QRX/AURA/REMOTE-PAYLOAD/V1`
- `QRX/AURA/REMOTE-LEASE/V1`

The new protocol uses explicit canonical big-endian integer encoding, bounded frames, requester-signed requests and provider-signed responses. Request/response semantics bind provider/pod identity, admission commitment, lease identity, sequence, job node, fragment contract, runtime/model/version, model commitment and payload commitment.

The client checks response binding and bounded response-height freshness. Phase 163 pins the remote-frame commitment golden vector:

`47cfe8c5095fc8cc1eeef65f728f62076e48b4290320325f3e78c2e08b8f70ce`

Signature tampering and payload tampering are rejected in the regression suite.

## Reservation lease audit

Remote provider reservations support `OPEN`, `RENEW`, `RELEASE` and expiry/reap behavior against the real `QrxResourceProviderRuntime` reservation API.

The lease contract binds immutable execution data including requester/admission/provider/pod identity, model/runtime identity and commitment, node ID, compute/network reservation, required memory, maximum output and fragment index/count. Exact retry is idempotent only for equivalent immutable lease semantics; a changed immutable request under the same lease/sequence is rejected. Renewal requires a higher sequence and matching immutable contract. Release is idempotent and expiry returns runtime resources.

The provider server reaps expired leases before request handling. In-flight leases remain process-local in this release; crash-durable lease journaling is explicitly deferred.

## Remote runtime dispatch audit

The distributed dispatch adapter creates one worker path per admitted pod and performs signed remote request/response exchange over TCP. The wire protocol is transport-authenticated by QRX signatures rather than treating TCP as identity.

Phase 163 includes a real non-Windows loopback TCP regression covering reserve -> remote execute -> signed result -> release. This is a functional transport regression, not an Internet/WAN performance benchmark.

The generic core intentionally does not invent tensor/expert splitting. Multi-pod jobs require explicit model/runtime-specific fragment and aggregate callbacks.

## QRX Drive model-fetch audit

0.0.9.38 adds a QRX Drive/CAS model-bundle abstraction for weights, tokenizer, config, expert packs and expert manifests. Model object transfer uses range-fetch adapters and inserts into local QRX Storage FS through content-addressed verification.

The bundle is checked against the selected `QrxAiModelRecord`, including model/version and tokenizer/expert-manifest commitments where applicable. Deterministic pod/model selection is used to keep model/expert placement stable and improve cache locality.

This release provides the verified fetch/cache adapter. It does not yet automatically trigger a Drive fetch for every remote provider cache miss.

## Anti-entropy convergence audit

AURA gossip now has order-independent digest roots for pod state, model-profile state and the combined live view. Equal sequence with conflicting signed content is treated as equivocation. Expired or not-yet-live source entries are skipped instead of poisoning reconciliation.

The node protocol adds `AURA_SYNC_DIGEST` and a bounded periodic anti-entropy tick. Peers compare digests first and perform paginated signed/authorized pull+merge only when state diverges. Merged state is persisted back to AURA gossip caches.

The anti-entropy interval is configurable as `aura_anti_entropy_interval_seconds`, defaults to 15 seconds and is clamped to 5..3600 seconds. Outbound HELLO signing uses a process-lifetime key cache to avoid repeated wallet-passphrase prompts and clears that cached key at shutdown.

Phase 163 verifies deterministic two-way convergence, stale-entry handling and equal-sequence equivocation rejection. It is not an Internet-scale convergence or partition-chaos benchmark.

## DeepSeek-compatible reasoning lane

DeepSeek compatibility is implemented as a normal AURA model capability/profile choice rather than a consensus special case. Phase 163 uses synthetic registered profiles to prove routing behavior: a simple chat remains on a smaller Qwen-compatible edge profile while a high-complexity reasoning request escalates to a distributed DeepSeek-compatible reasoning profile when the synthetic live fabric satisfies its requirements.

This is an architectural routing regression, not a real-world DeepSeek throughput claim. Concrete model version, runtime, weights and `license_id` remain registry metadata and must be supplied for the exact model being deployed.

## Known boundary

0.0.9.38 does not yet claim:

- a permanently configured autonomous compute-provider listener launched from normal node configuration;
- crash-durable in-flight lease recovery;
- automatic model fetch on every remote cache miss;
- a confidential/encrypted AURA payload layer beyond authentication/integrity;
- generic model-independent tensor/expert fragmentation for arbitrary MoE models;
- Internet-scale gossip convergence, partition-healing or throughput benchmarks;
- bundled DeepSeek model weights or a hard-coded DeepSeek consensus dependency.

These boundaries are deliberately carried into the next phase.

## Packaging acceptance gates

The canonical full-tree ZIP is accepted only if:

- no 0.0.9.37 baseline file is removed;
- generated `build`, `build-sanitize` and `cmake-build-*` directories are absent from the archive;
- `GUIWALLET/src/index.html` is present;
- remote dispatch source/header and Phase-163 test are present;
- root/core implementation notes, release audit and roadmap are present;
- clean-build and 96/96 CTest logs are present;
- the full-tree SHA-256 manifest verifies;
- `unzip -t` reports no archive errors.

## Next

`0.0.9.39 – Autonomous AURA Provider Service, Durable Lease Journal, Secure Remote Payload Channel & Model-Aware MoE Fragmentation`
