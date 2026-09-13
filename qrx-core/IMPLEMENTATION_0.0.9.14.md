# QRX 0.0.9.14 – Heterogeneous MoE Pods, Traffic Optimization & Optimistic PoUC

Implemented foundation:

- heterogeneous ARM64 / x86-64 MoE node profiles
- memory classes MICRO / LIGHT / STANDARD / HOT
- capability masks for CPU/GPU/NPU/Metal/MLX/CUDA/NEON/AVX2/AVX512/BF16
- role masks for routing, expert execution, verification, pre/post-processing,
  small-model jobs, caching and pod aggregation
- variable expert residency: a node may provide only one fragment; there is no
  fixed minimum number of experts per client
- expert fragment identity: model/version/layer/expert/fragment/content root
- RAM / VRAM / NVMe / remote residency states
- HOT / WARM / COLD cache state
- replica ordinal and pod binding
- deterministic SHA3-256 placement commitments
- topology-aware placement score using latency, bandwidth, residency and cache state
- activation wire-size accounting for FP16/BF16/FP8/INT8/INT4
- optimistic PoUC policies FAST / STANDARD / HIGH / CONSENSUS
- reliability-sensitive challenge rates
- asynchronous verification / optimistic-forward flag for non-consensus tiers
- strict consensus tier retains synchronous full verification semantics

Design rules:

1. Prefer full-expert placement before splitting one expert across nodes.
2. Use tensor/expert fragmentation only when a whole expert does not fit.
3. Model weights should be cached/resident; hot-path traffic should primarily
   carry activations/results rather than repeatedly transporting weights.
4. Keep latency-sensitive expert traffic within a pod/locality domain whenever possible.
5. Optimistic PoUC reduces verification overhead; it cannot remove the model's
   mandatory forward-pass communication.
6. Settlement/finality remains distinct from low-latency inference forwarding.
7. No claim is made that Kimi K3 inference is already operational in this phase.

Focused validation:

- compute_phase134_aura_runtime PASS
- compute_phase135_aura_chat PASS
- compute_phase136_aura_artifacts PASS
- compute_phase137_aura_code_workspace PASS
- compute_phase138_aura_code_lifecycle PASS
- compute_phase139_moe_traffic_optimistic PASS

A new full historical test-suite pass is not claimed here.
