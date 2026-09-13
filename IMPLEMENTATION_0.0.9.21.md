# QRX 0.0.9.21 – Compute Opportunity Engine

Implemented 2026-09-09 on top of the recovered full 0.0.9.20 project tree.

## Scope

Adds a deterministic, privacy-safe recommendation engine above the 0.0.9.20 Resource Globe. It converts regional demand/scarcity summaries plus a user's locally available hardware into bounded resource recommendations. It does not promise earnings, utilization, or future demand.

### Implemented
- versioned host capability/profile ABI
- storage, compute, AI accelerator, model-cache and network recommendation actions
- host-resource clamps: recommendations never exceed locally declared available resources
- regional Storage/Compute/AI/Model Cache/Network opportunity scores carried into the recommendation
- deterministic expected-utilization planning estimate derived from demand/utilization/opportunity; explicitly not an earnings guarantee
- bounded recommendation tiers:
  - storage up to 4 TiB according to opportunity, clamped by host availability
  - model cache up to 1 TiB, including a 500 GiB high-opportunity target, clamped by host availability
  - compute up to 12 threads in V1, clamped by available threads
  - network egress target up to 2.5 Gbit/s, clamped by host capability
- AI accelerator recommendation when regional AI opportunity is meaningful and the host has accelerator memory
- CUDA and Metal/MLX capability signaling kept distinct
- suitable for a Tesla P40 host profile: CUDA can be recommended without pretending Tensor Core support
- deterministic primary-action selection by highest applicable opportunity score
- privacy guard: public Opportunity Engine refuses sparse/non-public Resource Globe cells
- SHA3-256 domain-separated `QRX/OPPORTUNITY/RECOMMENDATION/V1` commitment

## Important boundaries
- Recommendations are network-planning hints, not revenue forecasts or guarantees.
- No exact provider coordinates, IPs, hostnames, or addresses are introduced.
- No automatic provider enrollment or resource allocation is performed in this phase.
- No token reward amount is calculated in this phase.
- CUDA/Metal/MLX execution remains delegated to the runtime adapters introduced in earlier 0.0.9.x phases.

## Test
`compute_phase146_opportunity_engine`

Full registered CTest suite after integration: **79/79 PASS**.
Known pre-existing compiler warnings in older QRX-Net/Core files remain and are not claimed fixed here.
