# QRX 0.0.9.17 — MoE Adaptive Batch Scheduler, Backpressure & End-to-End Throughput Telemetry

Status: FOUNDATION IMPLEMENTED.

Implemented in `src/compute/qrx_compute.h/.c` with focused test `tests/compute_phase142_moe_adaptive_scheduler_telemetry.c`.

## Adaptive batch scheduling
- Deterministic integer-only scheduling policy.
- Explicit minimum / target / maximum compatible batch sizes.
- Maximum queue wait and deadline guard.
- WAIT / DISPATCH_NOW / REROUTE / REJECT decisions.
- Queue and in-flight pressure tracked in basis points.
- NORMAL / ELEVATED / HIGH / CRITICAL pressure states.
- Deadline urgency and oldest-item age override batching delay.
- Critical overload reroutes when an alternate replica exists; otherwise rejects rather than silently overcommitting the worker.

## Replica backpressure and rerouting
- Candidate score combines queue pressure, in-flight pressure, reliability, residency, cache state, bandwidth and latency.
- Deterministic tie breaking by node ID.
- No model-router decision is changed: this chooses among valid replicas of the already-selected expert/fragment.

## End-to-end throughput telemetry
- Per-pod bounded measurement window.
- Completed requests and tokens.
- Activation bytes sent / result bytes received.
- Local aggregation bytes and WAN bytes.
- Compute time and queue-wait time.
- Batch, reroute, reject and failure counters.
- Fixed-point requests/sec and tokens/sec (`milli` units).
- WAN share in basis points.
- Domain-separated SHA3-256 telemetry commitment: `QRX/MOE/THROUGHPUT/V1`.

## Correctness boundaries
- Telemetry is measurement metadata, not proof that useful work was correct; PoUC receipts/challenges remain separate.
- Scheduler decisions do not alter model expert selection.
- This phase does not claim production transport, MLX/CUDA execution, or measured Kimi K3 throughput on real hardware.
- Backpressure is explicit; overloaded nodes are not allowed unbounded queue growth.

## Focused validation
Focused compute regression phases 134–142: 9/9 PASS.
No new complete historical full-suite pass is claimed.

Next: 0.0.9.18 — Runtime Transport Adapters & Reproducible MoE Throughput Benchmark Harness.
