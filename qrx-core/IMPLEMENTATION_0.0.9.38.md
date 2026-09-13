# QRX 0.0.9.38 – AURA Remote Dispatch Wire Protocol, Reservation Leases, QRX Drive Model Fetch & Anti-Entropy Convergence

Date: 2026-09-10
Status: implemented, release-audited and packaged

## Goal

Close the remaining gap between 0.0.9.37 local/live admission and actual network execution. AURA can now transport a signed admission to a remote provider, reserve that provider's real runtime resources under a lease, dispatch work over a bounded authenticated wire protocol, retrieve model objects from QRX Drive/CAS, and reconcile live pod/model gossip through deterministic anti-entropy.

The release also formalizes a DeepSeek-compatible reasoning lane without hard-coding DeepSeek into consensus. AURA continues to route by registered model capability and measured fabric readiness.

## New remote-dispatch module

New files:

- `qrx-core/src/compute/qrx_aura_remote_dispatch.h`
- `qrx-core/src/compute/qrx_aura_remote_dispatch.c`
- `qrx-core/tests/compute_phase163_aura_remote_dispatch_leases_drive_anti_entropy.c`

The module provides:

- signed remote request/response frames;
- deterministic payload and frame commitments;
- TCP framed transport helpers;
- provider-side resource reservation leases;
- client-side multi-pod dispatch adapter implementing the existing `QrxAuraDistributedDispatchFn` contract;
- QRX Drive/CAS model-bundle fetch;
- deterministic per-pod model/expert-object selection;
- gossip digest and anti-entropy merge support.

## Remote wire protocol

The canonical frame commitment domain is:

`QRX/AURA/REMOTE-FRAME/V1`

Payload bytes are separately bound under:

`QRX/AURA/REMOTE-PAYLOAD/V1`

Lease IDs are derived under:

`QRX/AURA/REMOTE-LEASE/V1`

The V1 frame includes signer/provider/pod identities, sequence, chain height, admission commitment, lease ID/expiry, requested compute/network resources, job node, memory/output bounds, fragment index/count, runtime/model identity, model commitment, payload commitment and payload bytes.

All numeric fields use explicit big-endian serialization. The encoded wire message is bounded. Payloads are capped at 16 MiB and signatures at 16 KiB.

The Phase-163 golden frame commitment is:

`47cfe8c5095fc8cc1eeef65f728f62076e48b4290320325f3e78c2e08b8f70ce`

## Request/response authentication

The requester signs every remote frame. The provider resolves the requester identity, verifies the frame signature, checks height freshness and verifies that the request targets the local provider/pod.

The provider signs every response. The client verifies the provider identity/key and rejects a response if its sequence, kind, provider/pod, admission, lease, job node, fragment or model commitment does not match the request. Response height is also bounded against the client's current height.

The remote transport does not treat TCP transport security as identity. The signed QRX frame is authoritative.

## Reservation leases

`LEASE_OPEN`, `LEASE_RENEW` and `LEASE_RELEASE` operate on `QrxResourceProviderRuntime` reservations.

An accepted lease binds the complete immutable execution identity:

- requester;
- admission commitment;
- provider and pod;
- model commitment;
- runtime/model/version;
- job node;
- compute threads;
- network egress reservation;
- required device memory;
- max output size;
- fragment index/count.

A same-sequence lease retry is idempotent only when all immutable fields and the requested expiry are identical. A changed request with the same lease ID/sequence is rejected rather than silently treated as a retry.

Renewal requires a strictly increasing sequence and may only extend expiry inside the maximum lease window; it cannot mutate the lease's immutable execution contract. Release returns provider runtime resources exactly once. Expired leases are reaped and their reservations returned.

The remote server reaps expired leases before serving the next request.

## Remote dispatch

A distributed admission may contain one or more pods. The client allocates a deterministic three-message sequence range per pod:

1. lease open;
2. dispatch;
3. lease release.

For multi-pod admissions, fragmentation and aggregation remain explicit adapters. The core does not pretend that arbitrary model tensors can be split generically. This keeps model-specific MoE/expert routing outside the generic authenticated transport.

The Phase-163 test opens an actual loopback TCP socket and executes:

`client -> signed lease open -> provider runtime reserve -> signed dispatch -> worker adapter -> signed result -> signed release`

The returned payload is verified and the real provider runtime reservation is back to zero after release.

The Windows build contains the transport/thread implementation, while the loopback socket subtest is currently compiled only on non-Windows hosts.

## QRX Drive model bundles

`QrxAuraModelBundleManifest` describes content-addressed model objects with types:

- weights;
- tokenizer;
- config;
- expert pack;
- expert manifest.

The bundle itself is content addressed. The `QrxAiModelRecord.manifest_root` must equal the bundle root, and model ID/version, tokenizer root and (for MoE) expert-manifest root must match the authoritative registry record.

Objects are fetched through generic QRX Drive/CAS `stat` and range-fetch callbacks. Each fetched object is inserted into the local QRX storage filesystem and must reproduce its declared content root. A tampered registry tokenizer root is rejected in Phase 163.

Control metadata is fetched for every execution pod. Large weight/expert objects are then selected deterministically using the pod ID plus model commitment. This gives different pods stable cache affinity and avoids requiring every MoE pod to download every expert pack.

The fetch API is reusable by the live cache-plan executor, but 0.0.9.38 does not yet run a permanent autonomous model-cache daemon loop.

## Anti-entropy convergence

0.0.9.38 extends AURA gossip with `QrxAuraGossipDigest`:

- live pod count;
- live model count;
- order-independent pod root;
- order-independent model root;
- combined root.

Domains:

- `QRX/AURA/ANTI-ENTROPY/PODS/V1`
- `QRX/AURA/ANTI-ENTROPY/MODELS/V1`
- `QRX/AURA/ANTI-ENTROPY/COMBINED/V1`

Only currently live announcements contribute to the digest. Stale/not-yet-live source entries are ignored during merge instead of poisoning convergence.

Equal sequence plus different signed content is treated as equivocation rather than a normal merge.

The node protocol adds `AURA_SYNC_DIGEST`. The node periodically compares its digest with a small rotating set of known peers. On divergence and within a bounded height delta it performs paginated `AURA_POD_PULL` / `AURA_MODEL_PULL`, re-verifies signatures and authorization, merges newer records, and persists changed caches.

Anti-entropy interval is configurable through `aura_anti_entropy_interval_seconds` and is clamped to a safe 5–3600 second range. The default is 15 seconds.

A process-lifetime outbound HELLO key cache prevents periodic anti-entropy from repeatedly requesting the wallet passphrase after the key has first been loaded; keys are freed/cleansed on node shutdown.

## DeepSeek-compatible reasoning integration

DeepSeek support is deliberately implemented as a model-profile/runtime choice, not a consensus special case.

A signed AURA model profile can advertise a DeepSeek-compatible family with a high `reasoning_bps`, memory/storage requirements, MoE flag, minimum pod count, throughput, network, cache and expert-locality requirements. The existing AUTO router can therefore escalate a hard `QRX_AURA_TASK_REASONING` request from a smaller Qwen-compatible edge profile to a larger DeepSeek-compatible distributed reasoning profile when the live fabric satisfies it.

Phase 163 explicitly verifies:

- an easy chat request stays on the smaller Qwen-compatible profile;
- a high-complexity reasoning request escalates to the DeepSeek-compatible profile;
- the selected DeepSeek-compatible profile requires at least two live pods in the synthetic test fabric.

This architecture also allows other reasoning families (Kimi, Qwen, Llama-derived, future AURA-native models) to compete under the same capability profile. No vendor/model name changes consensus rules.

Licensing is not inferred from the family name. Each concrete model/version remains responsible for its own model-registry `license_id` and signed registry commitment.

## Phase 163 validation

`compute_phase163_aura_remote_dispatch_leases_drive_anti_entropy` verifies:

- canonical remote-frame golden commitment;
- signed wire round-trip;
- signature tamper rejection;
- payload tamper rejection;
- real resource reservation on lease open;
- exact-retry idempotence;
- same lease/sequence with changed immutable fields is rejected;
- strict renewal sequence and immutable-field enforcement;
- exact release idempotence;
- expiry reaping and resource return;
- real loopback TCP remote dispatch on non-Windows hosts;
- provider-signed response verification;
- QRX Drive model manifest/object range fetch into local CAS;
- model/tokenizer registry-root enforcement;
- order-independent AURA gossip digest convergence;
- two-way anti-entropy merge;
- equal-sequence equivocation rejection;
- stale source records are ignored;
- Qwen-compatible chat routing and DeepSeek-compatible reasoning escalation.

## Scope boundary

0.0.9.38 provides the authenticated transport/library path and periodic gossip anti-entropy in the node, but does not yet claim all of the following:

- a fully autonomous daemon-managed remote compute listener instantiated from wallet/provider configuration;
- durable persistence/recovery of in-flight reservation leases across a provider process crash;
- automatic remote model fetch triggered by every cache miss inside the wire server;
- production model-specific tensor/expert fragmentation/aggregation for every supported architecture;
- end-to-end WAN stress/convergence testing across hundreds or thousands of real nodes;
- encrypted application payload confidentiality at the AURA frame layer (frames are authenticated; privacy-sensitive workloads still require the intended secure transport/encryption layer);
- production performance numbers for DeepSeek, Qwen, Kimi or any named model/hardware combination.

Container validation uses `QRX_BUILD_TESTS=ON` and `QRX_REQUIRE_PQC=OFF`; this remains a developer/test build mode and does not relax production/Mainnet PQC policy.

## Next candidate

`0.0.9.39 – Autonomous AURA Provider Service, Durable Lease Journal, Secure Remote Payload Channel & Model-Aware MoE Fragmentation`
