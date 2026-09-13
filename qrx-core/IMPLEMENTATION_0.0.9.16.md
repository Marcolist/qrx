# QRX 0.0.9.16 — MoE Micro-Batching, Persistent Pod Streams & Hierarchical Aggregation

Status: FOUNDATION IMPLEMENTED.

Implemented in `src/compute/qrx_compute.h/.c` with focused test `tests/compute_phase141_moe_batch_stream_aggregation.c`.

## Micro-batching
- Deterministic batches group only compatible `(layer, expert, fragment, activation encoding)` work.
- Explicit caps for request count, tokens and estimated wire bytes.
- Earliest-deadline-first canonical order.
- Duplicate request IDs rejected.
- Activation wire accounting uses the 0.0.9.14 encoding model.
- Domain-separated SHA3-256 commitment: `QRX/MOE/MICROBATCH/V1`.

## Persistent pod streams
- Protocol state for long-lived coordinator↔worker streams.
- OPEN/DRAINING/CLOSED/FAILED lifecycle.
- In-flight batch ceiling and monotonic byte counters.
- Feature bits prepare compression, zero-copy, RDMA and multiplexing.
- This phase does not claim a production QUIC/JACCL/RDMA transport implementation.

## Hierarchical aggregation
- Pod-local partial results can be collected by a designated aggregator.
- Deterministic sequence order and duplicate-node/sequence rejection.
- Local input bytes vs single uplink-result bytes are accounted separately.
- `qrx_moe_aggregation_wire_savings()` exposes traffic avoided outside the pod.
- Domain-separated SHA3-256 commitment: `QRX/MOE/AGGREGATION/V1`.

## Security / correctness boundaries
- Micro-batching never changes the model-selected expert identity.
- Batching is rejected across incompatible expert/fragment/encoding targets.
- Persistent streams are transport metadata; they do not grant host/network privileges to AURA jobs.
- Hierarchical aggregation does not weaken PoUC policy; receipts/challenges remain independent.

Next: 0.0.9.17 — MoE Adaptive Batch Scheduler, Backpressure & End-to-End Throughput Telemetry.
