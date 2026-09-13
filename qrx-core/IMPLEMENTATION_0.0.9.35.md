# QRX 0.0.9.35 – Adaptive AURA Model Fabric, Edge AI Bootstrap & Pod Capacity Registry

Date: 2026-09-10
Status: Implemented and validated in developer/test mode

## Goal

Make AURA useful before QRX has a large distributed AI fleet. A single calibrated edge device may contribute immediately, while the same routing layer can scale through larger local pods and eventually distributed K2/K3-class MoE capacity.

The protocol is deliberately benchmark-driven. Product names such as Raspberry Pi, ODROID, Jetson/Orin, MacBook, Ryzen AI, Tesla P40, or other ARM/x86 systems do not grant a tier. Eligibility is derived from measured runtime capability, available memory, throughput, latency, network capacity, reliability, cache state, and—when relevant—expert locality.

## New model-fabric module

New files:

- `qrx-core/src/compute/qrx_aura_model_fabric.h`
- `qrx-core/src/compute/qrx_aura_model_fabric.c`
- `qrx-core/tests/compute_phase160_aura_adaptive_model_fabric.c`

The module introduces:

- pod-capacity registry with deterministic upsert/remove semantics;
- model capability profiles independent of model vendor naming;
- scheduling tiers `NANO`, `EDGE`, `LOCAL`, `CLUSTER`, `K2_CLASS`, and `K3_CLASS`;
- pod roles for inference, routing, embeddings, RAG, pre/post-processing, verification, and model cache;
- bridge from the existing native MoE calibration profile into AURA pod capacity;
- AUTO and PINNED model routing;
- graceful quality degradation when explicitly allowed;
- privacy-safe Resource Globe AI aggregation;
- deterministic SHA3-256 route-decision commitments;
- binding of the selected model/runtime requirements into `QrxComputeJobNode`.

## Bootstrap tiers

Per-device bootstrap classification is based on free memory and measured inference throughput:

- utility-only pods can still qualify for `NANO` service when they have enough memory for routing/pre-post/verification/cache work;
- `NANO`: >=2 GiB free memory and >=0.25 measured tokens/s equivalent;
- `EDGE`: >=6 GiB free memory and >=2 measured tokens/s equivalent;
- `LOCAL`: >=16 GiB free memory and >=8 measured tokens/s equivalent;
- `CLUSTER`: local multi-node pod or >=32 GiB free memory plus >=20 measured tokens/s equivalent.

`K2_CLASS` and `K3_CLASS` are network readiness classes, not per-device labels.

These thresholds are scheduling bootstrap policy, not claims about any named hardware model.

## Model capability profiles

A model profile can require or advertise:

- model ID/version/family/runtime and quantization;
- minimum and recommended memory;
- minimum storage and aggregate throughput;
- minimum pod count;
- network throughput and latency bounds;
- reliability, cache-hit, and expert-locality floors;
- context window;
- task quality for general quality, coding, reasoning, RAG, and tool use;
- accelerator feature requirements;
- MoE vs dense behavior and single-device support.

This permits small Qwen/Gemma/Llama-class profiles at bootstrap and larger distributed K2/K3-class profiles later without hard-coding a single model family into consensus scheduling.

## AUTO routing

AUTO routing selects the smallest fully ready model tier that satisfies the requested task quality and context constraints. This avoids spending K2/K3-class distributed capacity on tasks that a small local model can handle.

If no model satisfies the requested quality and `allow_degradation` is set, the router chooses the highest-quality fully ready model and marks the decision as degraded. It never silently claims a stronger model than the one actually selected.

PINNED routing is fail-closed: an explicitly pinned model must be 100% ready and satisfy context constraints, otherwise routing fails instead of silently downgrading.

## Determinism and commitment

The route decision is committed under:

`QRX/AURA/MODEL-ROUTE/V1`

All integer fields are serialized explicitly in big-endian order and strings are length-framed before SHA3-256 hashing. Single-device candidate ties use deterministic ordering down to `pod_id`, so input ordering does not change the selected pod.

Large readiness ratios use bounded integer arithmetic; no floating-point fallback is required for oversized counters.

## Resource Globe integration

The fabric produces aggregate AI cells and a global snapshot containing:

- provider and pod counts;
- inference and utility pod counts;
- per-tier pod counts;
- free memory and model-cache capacity;
- aggregate measured AI throughput;
- average latency, utilization, reliability, cache-hit rate, and expert locality;
- K2/K3 readiness scores;
- maximum currently ready model tier.

Regional cells preserve the existing privacy policy: a region becomes publicly visible only when the configured minimum number of providers is present. Provider IDs and pod IDs remain internal inputs rather than public Globe identity fields.

## Small-device contribution

Phase 160 deliberately includes synthetic low-power ARM profiles to prove protocol behavior:

- a Pi-5-like calibrated profile can participate in a small NANO/EDGE workload when its measured capability satisfies the profile;
- an ODROID-N2+-like profile can participate at NANO/utility level;
- utility-only low-memory nodes remain useful for routing, pre/post-processing, verification, and cache work even when they are unsuitable for primary token generation.

The synthetic test numbers are not benchmark claims for those products. Real deployments must use QRX runtime calibration on the actual device/model/quantization/backend.

## Phase 160 validation

`compute_phase160_aura_adaptive_model_fabric` verifies:

- benchmark-driven Pi-like and ODROID-like participation without a product allowlist;
- CUDA, MLX/Metal, generic CPU, generic accelerator and P40-style backend profiles;
- tier classification;
- K2 full readiness and partial K3 readiness from the same pod fleet;
- Resource Globe regional privacy threshold behavior;
- AUTO routing to NANO, EDGE and K2-class models by task complexity;
- deterministic route commitment golden vector;
- model/runtime binding into a compute job node;
- fail-closed K3 PINNED routing when readiness is insufficient;
- graceful degradation to a local model after stronger pods disappear;
- utility-only pod eligibility;
- native calibration-to-pod bridge.

## Validation boundary

The release validates the model-fabric core and deterministic scheduling behavior. It does **not** claim live decentralized pod gossip, live Resource Globe telemetry, automatic distribution of model profiles/weights, or real-world performance for named hardware. Those are intentionally the next integration layer.

Container validation uses `QRX_BUILD_TESTS=ON` and `QRX_REQUIRE_PQC=OFF`. This is a developer/test build mode and does not relax QRX production/Mainnet PQC requirements.

## Next

`0.0.9.36 – AURA Pod Gossip, Live Resource Globe AI Telemetry & Model Profile Distribution`
