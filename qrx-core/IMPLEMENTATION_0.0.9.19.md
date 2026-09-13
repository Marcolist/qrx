# QRX 0.0.9.19 — Native Runtime Discovery, CUDA/MLX Worker Adapter & Calibration Benchmarks

Status: DONE — FOUNDATION

## Native runtime discovery

QRX Compute can now build a runtime inventory from the local machine without a build-time CUDA dependency.

- CPU is always represented as a native worker device with detected system memory and online CPU count.
- NVIDIA CUDA devices are discovered dynamically through the CUDA Driver API (`libcuda.so(.1)` on Unix/Linux or `nvcuda.dll` on Windows) when the driver is present.
- CUDA discovery records device name, device memory and compute capability.
- NVIDIA Pascal/P40 is therefore discovered as SM 6.1 when the installed driver reports it. P40 never inherits the tensor-core feature bit.
- Apple Silicon is represented as native Metal/unified-memory capable on arm64 macOS.
- MLX execution is deliberately **not** inferred simply because the host is Apple Silicon. A real MLX worker adapter must be present before QRX may mark MLX execution ready.

No GPU serial number, hostname or other unnecessary hardware identifier is added to consensus metadata.

## Runtime worker adapter contract

Added a backend-neutral worker adapter callback contract. Before execution QRX validates:

- runtime backend match
- required kernel feature mask
- requested batch size
- required device memory
- device eligibility
- adapter readiness

This makes SM-aware CUDA dispatch possible. A P40 can accept compatible FP32/FP16/INT8 paths but is rejected for a tensor-core-only requirement.

The contract also allows an external MLX/Metal worker to be attached without pretending that MLX is already linked into qrxcore.

## Calibration profiles

Added measured runtime calibration profiles containing:

- device profile
- measured memory bandwidth
- measured compute operations per second
- measured model milli-token/s
- model batch size
- sample count
- measured transport latency
- measured transport bandwidth

Real model executions can feed calibration samples using completed-token and elapsed-time measurements. Transport probes can record payload size and RTT and derive latency/bandwidth. A validated profile can replace the synthetic `standalone_milli_tokens_per_second` input of the 0.0.9.18 benchmark node.

Profiles are bound by domain-separated SHA3-256 `QRX/MOE/CALIBRATION/V1` commitments.

## Important boundary

This milestone provides native CUDA **discovery** and the execution-adapter contract. It does not include CUDA/MLX model kernels or claim that Kimi K3 inference is already running on P40/Apple hardware. Those runtimes must attach to the adapter and produce real calibration samples before performance claims are allowed.

## Tests

New test: `compute_phase144_native_runtime_calibration`

Covered:
- native CPU inventory discovery
- runtime inventory validation
- P40 SM 6.1 kernel eligibility
- explicit rejection of tensor-core-only work on P40
- worker adapter execution gating
- real-measurement calibration sample math
- transport probe calibration math
- deterministic calibration commitment
- calibrated benchmark-node replacement

Focused regression compute phases 134–144: 11/11 PASS.

No fresh full historical suite is claimed.
