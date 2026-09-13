# QRX 0.0.9.2 — Standardized Compute Benchmark / Normalized Compute Units

Implemented on top of QRX 0.0.9.1 capability discovery.

## Added

- Versioned `QrxComputeBenchmarkMetrics` protocol schema
- Standard benchmark dimensions matching the roadmap:
  - BF16 matrix/arithmetic throughput
  - MXFP4 matrix/arithmetic throughput
  - memory bandwidth
  - storage streaming throughput
  - network throughput
  - MoE expert execution
  - model inference throughput
- Deterministic fixed-point normalization; no floating-point score calculation
- `10,000` score points define the version-1 baseline `1.0 QRX Normalized Compute Unit (NCU)`
- Per-component score cap at `20,000` to prevent one reported dimension from dominating the scheduler
- Version-1 weighted score:
  - CPU arithmetic: 30%
  - memory: 15%
  - storage: 10%
  - network: 10%
  - MoE expert execution: 20%
  - model inference: 15%
- Scheduler classes: ENTRY, STANDARD, HIGH, EXTREME
- Validation rejects incomplete benchmark source masks, zero metrics, invalid versions and absurd sampling metadata
- Reference constants are explicitly calibration values, not claims about real hardware performance and not automatic reward entitlements

## Security / economic boundary

0.0.9.2 defines the benchmark schema and deterministic normalization only. A provider cannot earn money merely by self-reporting a high score. Verified benchmark provenance, challenge/attestation and anti-spoofing belong to later compute-market verification work. Network and storage measurements therefore require trusted/verified measurement backends before they can influence paid scheduling in production.

Likewise, the portable Core does not pretend to provide optimized BF16/MXFP4 kernels on hardware where a verified backend has not been implemented. The schema is ready for those backend measurements; 0.0.9.1 runtime dispatch remains the authoritative local capability source.

## Verification

RelWithDebInfo developer regression build with `QRX_BUILD_TESTS=ON`:

- 61 / 61 tests passed
- existing 0.0.8 regression baseline preserved
- `compute_phase126_foundation` passed
- `compute_phase127_capability_discovery` passed
- new `compute_phase128_benchmark_normalization` passed

Developer build used `QRX_REQUIRE_PQC=OFF` because this container is not the QRX OpenSSL/PQC release toolchain. Production PQC requirements are unchanged.

## Next

0.0.9.3 — QRX AI Model Registry

- on-chain model identity/version metadata
- QRX Drive roots for large model/tokenizer/expert data
- architecture/runtime requirements
- memory/storage requirements
- verification profile
- license metadata
- Kimi K3 as the first high-end MoE target model, without hard-coding unsupported benchmark claims
