# QRX 0.0.9.15 – MoE Pod Coordinator, Expert Co-Activation Locality & Predictive Prefetch

Status: FOUNDATION IMPLEMENTED

## Scope

This milestone adds a deterministic coordinator layer above the 0.0.9.14 heterogeneous MoE placement primitives. It does not alter model router choices and does not claim live Kimi K3 inference. It provides the metadata, scoring and bounded prefetch-plan machinery needed to keep selected and likely-next experts close to the pod executing a request.

## Implemented

- Versioned co-activation statistics keyed by model/version/layer/expert pair.
- Deterministic basis-point co-activation score.
- Locality score extending the 0.0.9.14 placement score with bounded co-activation affinity.
- Versioned coordinator request binding pod/current layer/next layer/selected experts/prefetch count/prefetch byte ceiling/activation encoding.
- Duplicate selected-expert rejection.
- Bounded predictive prefetch plans.
- Prefetch actions for REMOTE->RAM, NVMe->RAM, RAM->VRAM and already-resident VRAM/no-op.
- Hard max-prefetch-item and max-prefetch-byte guards.
- Remote wire-byte accounting separate from local residency movement.
- Duplicate placement rejection.
- Deterministic canonical sort by locality score and stable tie breakers.
- Domain-separated SHA3-256 prefetch plan commitment: `QRX/MOE/PREFETCH-PLAN/V1`.

## Security / Correctness Rules

- Coordinator placement never changes the model's router-selected experts.
- Predictive prefetch is advisory cache placement only.
- No unbounded network fetch plan can be created through the API.
- Model fragments stay content-root bound.
- Integer-only deterministic scoring is used in protocol metadata.
- Prefetching does not imply correctness; execution remains subject to PoUC / optimistic challenge policy.

## Focused Regression

Configured with `-DQRX_BUILD_TESTS=ON`.

- compute_phase134_aura_runtime PASS
- compute_phase135_aura_chat PASS
- compute_phase136_aura_artifacts PASS
- compute_phase137_aura_code_workspace PASS
- compute_phase138_aura_code_lifecycle PASS
- compute_phase139_moe_traffic_optimistic PASS
- compute_phase140_moe_coordinator_prefetch PASS

Focused regression: 7/7 PASS.

The existing project still emits pre-existing compiler warnings in older modules. No fresh full historical suite is claimed by this milestone.
