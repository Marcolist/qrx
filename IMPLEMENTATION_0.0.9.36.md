# QRX 0.0.9.36 – AURA Pod Gossip, Live Resource Globe AI Telemetry & Model Profile Distribution

Date: 2026-09-10
Status: Implemented and validated in developer/test mode

## Goal

Turn the 0.0.9.35 AURA model-fabric snapshot into a signed network-distributable capability view. QRX nodes can ingest and relay current pod-capacity announcements and model capability profiles, build a live AURA scheduling snapshot from non-expired signed records, and expose privacy-filtered AI capacity through daemon/RPC-facing status methods.

This phase distributes metadata/capability profiles, not model weights and not user prompts/results.

## New gossip module

New files:

- `qrx-core/src/compute/qrx_aura_fabric_gossip.h`
- `qrx-core/src/compute/qrx_aura_fabric_gossip.c`
- `qrx-core/tests/compute_phase161_aura_pod_gossip_live_globe.c`

The module introduces two signed announcement types:

- `QrxAuraPodAnnouncement`
- `QrxAuraModelProfileAnnouncement`

Pod announcements carry a 0.0.9.35 `QrxAuraPodCapacity` plus sequence, validity window and service endpoint. Model-profile announcements carry a `QrxAuraModelCapabilityProfile`, publisher identity, sequence, validity window and a signed `model_registry_commitment` field.

## Canonical wire and hash format

Pod announcements are committed under:

`QRX/AURA/POD-GOSSIP/V1`

Model-profile announcements are committed under:

`QRX/AURA/MODEL-PROFILE-GOSSIP/V1`

All integers in the new V1 announcement serialization use explicit big-endian encoding. Strings are length framed. Phase 161 includes exact SHA3-256 golden vectors for both announcement classes.

The wire envelope is bounded to 64 KiB and includes an 8-byte message magic, payload length, canonical payload, signature length and signature.

## Signature and identity rules

Pod gossip is fail-closed:

- the provider identity in the pod capacity record is used for public-key lookup;
- the announcement signature must verify against that provider key;
- a newer announcement must have a strictly increasing sequence;
- an existing `pod_id` cannot be taken over by a different provider identity;
- expired/not-yet-valid announcements are rejected from live admission;
- unavailable pods are not admitted as live capacity.

In the 0.0.9.36 daemon integration, pod-key lookup reuses the already consensus-bound storage-provider discovery key lookup. This gives a real authenticated identity path now, but means a compute-only provider that has no storage-provider identity binding cannot yet independently publish a pod through this V1 daemon path. Native compute-provider identity binding is intentionally deferred to 0.0.9.37.

Model-profile gossip is signed by an authorized governance-root identity. The publisher must resolve to an active governance root and the signature must verify. The announcement cryptographically binds `model_registry_commitment`, but 0.0.9.36 does not yet prove that this commitment corresponds to an authoritative on-chain model-registry record.

## Gossip tables and persistence

The global live view supports:

- up to 4096 pod announcements;
- up to 256 model-profile announcements;
- monotonic per-record sequence handling;
- pruning by validity height;
- revision counters for local state changes;
- signed cache persistence across daemon restarts.

Cache files are transport caches only. On load, every cached entry is decoded and signature-verified again before admission. Stale entries are not trusted just because they existed on disk.

The existing small local scheduler registry remains capped separately at 256 pods; the 4096 limit belongs to the global gossip/live-fabric view.

## P2P integration

The node protocol now recognizes:

- `AURA_POD_PUSH`
- `AURA_MODEL_PUSH`
- `AURA_POD_PULL`
- `AURA_MODEL_PULL`
- `AURA_FABRIC_STATUS`

Accepted PUSH records are persisted and fanned out to a bounded number of known peers. Invalid records receive a peer reputation penalty. PULL is paginated and bounded. The status response is generated from the current signed/non-expired gossip tables rather than from a static hardware inventory.

This phase wires the protocol handlers and relay path, but does not claim a full autonomous anti-entropy protocol: periodic peer pull/synchronization, local automatic compute-provider self-publication and large-network convergence testing remain later work.

## Live Resource Globe and routing bridge

`qrx_aura_gossip_live_snapshot()` and `qrx_aura_gossip_live_route()` filter the gossip tables by current chain height and feed only current entries into the 0.0.9.35 model-fabric scheduler.

The Resource Globe bridge aggregates current pod announcements into privacy-safe region cells. Public output continues to hide individual provider IDs, pod IDs and service endpoints. A region is public only when the configured unique-provider threshold is met; the default remains 3 providers.

Global and regional telemetry includes the existing 0.0.9.35 aggregates such as pod/provider counts, measured AI throughput, model-cache capacity, latency, utilization, reliability, cache hit rate, expert locality and K2/K3 readiness.

## Daemon / CLI observability

`qrxd` now exposes:

- `getaurafabric`
- `getauraatlas`
- `listauramodels`

`qrx-cli` forwards the same commands.

`getaurafabric` reports the global live fabric snapshot. `getauraatlas` returns only privacy-visible regions. `listauramodels` reports currently loaded signed model capability profiles and their publisher/registry commitment metadata.

## Phase 161 validation

`compute_phase161_aura_pod_gossip_live_globe` verifies:

- Ed25519 signing and verification for pod/model announcements;
- canonical big-endian wire round-trip;
- exact pod and model-profile SHA3-256 golden vectors;
- replay rejection;
- forged-signature rejection;
- pod-ID provider takeover rejection;
- governance-authorized model publisher acceptance and unauthorized publisher rejection;
- live-fabric snapshot from signed current announcements;
- privacy threshold behavior across visible/hidden regions;
- Resource Globe bridge;
- AUTO routing from live gossip state, including escalation to a K2-class profile when the synthetic fleet satisfies it;
- signed cache save/reload with re-verification;
- expiry pruning.

The phase test is a deterministic module/integration test. It does not open multiple real node processes and therefore is not a WAN-scale gossip/convergence benchmark.

## Build warning cleanup

During release finishing, five new `-Wmisleading-indentation` diagnostics in `qrx_aura_fabric_gossip.c` and two in the new `qrx.c` AURA helper code were removed without changing behavior. The final clean build returns to the inherited historical warning baseline and emits no warning referencing the new gossip source or Phase-161 test.

## Validation boundary

0.0.9.36 does **not** yet implement:

- native compute-provider identity registration independent of storage-provider identity;
- automatic local pod advertisement generation from the calibrated runtime/provider process;
- periodic anti-entropy/pull convergence across arbitrary peer graphs;
- scheduler reservation/admission against a live pod before dispatch;
- model-weight/chunk distribution or cache placement;
- runtime job dispatch to the selected pod;
- authoritative on-chain validation of `model_registry_commitment`;
- a multi-process/socket stress test or Internet-scale gossip benchmark.

Container validation uses `QRX_BUILD_TESTS=ON` and `QRX_REQUIRE_PQC=OFF`. This is a developer/test build mode and does not relax QRX production/Mainnet PQC requirements.

## Next

`0.0.9.37 – AURA Live Scheduler Admission, Compute Provider Identity Binding, Model Cache Placement & Runtime Dispatch`
