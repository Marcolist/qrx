# QRX 0.0.9.18 — Runtime Transport Adapters & Reproducible MoE Throughput Benchmark Harness

Status: DONE — FOUNDATION

## Runtime transport contract

Added deterministic transport descriptors for LOCAL, SHM, TCP, QUIC, MPI and JACCL/RDMA classes. Each descriptor binds latency, bandwidth, maximum in-flight operations, MTU/message overhead and negotiated features (async, multiplexing, zero-copy, RDMA, compression).

This is a protocol/runtime contract. It does not claim that production QUIC, MPI or JACCL/RDMA I/O is already linked into the daemon.

## Runtime device contract

Added CPU, MLX/Metal, CUDA and generic backend descriptors with accelerator feature flags and device-memory limits.

NVIDIA Tesla P40 is explicitly representable as CUDA compute capability 6.1 / Pascal with 24 GiB device-memory profiles. P40 profiles do not advertise tensor cores. QRX therefore must select ordinary CUDA FP32/FP16/INT8-capable kernels that are valid for the backend rather than tensor-core-only kernels.

The P40 support in this milestone is capability + scheduling/benchmark foundation. Native CUDA kernels, CUDA runtime discovery and end-to-end P40 inference remain runtime-adapter work; they are not falsely claimed here.

## Reproducible benchmark harness

Added a deterministic synthetic benchmark scenario with:
- scenario ID + deterministic seed
- arbitrary heterogeneous node list (up to 256 nodes per scenario descriptor)
- per-node runtime backend and transport
- measured/declared standalone milli-token/s input
- availability basis points
- activation/result bytes per token
- concurrency + requested micro-batch size
- synthetic fault rate
- duration

The harness derives raw aggregate throughput, transport-amortized estimated throughput, token count, wire bytes, compute/transport time and the bottleneck transport index. It intentionally remains an estimator until calibrated with real hardware measurements.

A domain-separated SHA3-256 `QRX/MOE/BENCHMARK/V1` commitment binds scenario inputs and results so repeated benchmark profiles can be compared reproducibly.

## Tests

New test: `compute_phase143_moe_runtime_benchmark`

Focused regression: compute phases 134–143 = 10/10 PASS.

No fresh full historical suite is claimed.
