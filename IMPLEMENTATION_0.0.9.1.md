# QRX 0.0.9.1 — Cross-Platform Compute Runtime / Native Capability Discovery

Implemented on top of 0.0.9.0 Compute Foundation.

## Added

- Native OS detection: Linux, Windows, macOS
- Native architecture detection: x86-64, ARM64
- Logical CPU discovery
- Physical/available memory discovery
- Runtime x86 feature detection for SSE2, AVX2, FMA and AVX-512F
- ARM64 feature model for NEON plus compile-time SVE/SVE2/BF16 capability exposure
- Architecture-independent CPU feature bitset
- Deterministic runtime dispatch profile
- Preferred kernel classes: scalar, SSE2, AVX2, AVX-512, NEON, SVE, SVE2
- Conservative host-memory reservation for compute scheduling
- Privacy-preserving capability fingerprint remains coarse and excludes hostname, IP, serial numbers and exact free memory
- Backend GPU/NPU discovery intentionally remains plugin/backend work; 0.0.9.1 does not fabricate accelerators that cannot be safely detected by the portable core

## Security invariants preserved

- Compute tasks still cannot request arbitrary host commands
- Compute tasks still cannot request host filesystem writes
- Compute tasks still cannot request external network access
- Hardware detection does not expose unique device identifiers
- Capability discovery is local and does not itself grant execution permission

## Verification

RelWithDebInfo regression build with internal tests enabled:

- 60 / 60 tests passed
- existing compute foundation test passed
- new `compute_phase127_capability_discovery` passed

Developer build used `QRX_REQUIRE_PQC=OFF` only because the build host OpenSSL is not the project PQC release toolchain; this does not alter the production PQC requirement.

## Next

0.0.9.2 — Standardized Compute Benchmark / Normalized Compute Units:
MXFP4/BF16 kernels, memory bandwidth, storage streaming, network throughput, MoE expert execution and normalized QRX Compute Score.
