# QRX 0.0.9.39 Release Audit

Date: 2026-09-10
Release: Autonomous AURA Provider Service, Durable Lease Journal, Secure Remote Payload Channel & Model-Aware MoE Fragmentation

## Canonical predecessor

Baseline archive:

`qrx-core-0.0.9.38-aura-remote-dispatch-leases-drive-anti-entropy.zip`

The implementation tree was compared against a fresh extraction of that canonical predecessor with build directories excluded.

Pre-release implementation comparison before adding 0.0.9.39 release documentation:

- predecessor regular files: **1129**
- removed predecessor files: **0**
- added implementation files: **3**
- changed implementation files: **3**

Added implementation files:

- `qrx-core/src/compute/qrx_aura_provider_service.c`
- `qrx-core/src/compute/qrx_aura_provider_service.h`
- `qrx-core/tests/compute_phase164_aura_provider_service_secure_durable_moe.c`

Changed implementation files:

- `qrx-core/CMakeLists.txt`
- `qrx-core/src/compute/qrx_aura_remote_dispatch.c`
- `qrx-core/src/compute/qrx_aura_remote_dispatch.h`

The release additionally adds root/qrx-core implementation and audit documents, build/test logs, updated roadmap and the final full-tree manifest.

## Build audit

Configuration:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

A `--clean-first` build was performed. The first invocation reached about 57% and was terminated by the tool execution time limit. The same clean build directory was resumed without reconfiguration and reached **100%**, including `qrx`, `qrxd`, `qrx-cli`, wallet CLI, QRXDB tools and all configured test binaries.

This is recorded as a completed resumed clean build, not falsely described as a single uninterrupted build invocation.

Compiler warning audit:

- canonical 0.0.9.38 clean-build warning lines: **413**
- final 0.0.9.39 clean-build warning lines: **413**
- new 0.0.9.39 remote/provider/Phase-164 warning lines: **0**
- compiler error lines: **0**

The historical QRX tree is therefore not claimed warning-free.

## Test audit

After the final source adjustments and clean build, the entire registered suite was rerun:

- registered tests: **97**
- passed: **97**
- failed: **0**

Focused backward-compatibility check:

- Phase 163 remote dispatch/leases/Drive/anti-entropy: PASS
- Phase 164 provider service/secure durable MoE: PASS

This verifies that the extended server/client context and secure/durable functionality did not break the 0.0.9.38 regression path.

## Security audit boundary

### Authentication/integrity

Remote frames continue using the signed 0.0.9.38 frame protocol. The secure payload layer does not replace provider/requester signature validation.

### Confidentiality

AES-256-GCM encrypts secure request and response payloads. Frame metadata is authenticated as AAD. Random 96-bit GCM nonces are generated with OpenSSL `RAND_bytes`.

The test flips the authentication tag and verifies decryption failure.

### Key establishment

The core accepts an abstract session secret and provides an optional X25519 helper. **X25519 is classical and is not a post-quantum claim.** Mainnet post-quantum transport key establishment remains a follow-up requirement. The present session abstraction is intentionally compatible with feeding a later ML-KEM/hybrid shared secret into the same AES-256-GCM channel.

### Durable replay/reservation state

Lease journal payload is canonically encoded and SHA3-256 checked under `QRX/AURA/LEASE-JOURNAL/V1`. Save uses temp-file + durable flush + atomic replacement. Recovery re-reserves only still-live ACTIVE leases and rolls back partial runtime recovery on failure.

The dispatch sequence barrier is persisted before worker execution when durable leases are configured.

## Model/cache/MoE audit

A provider cache miss may auto-fetch only after authoritative model-registry validation of model ID/version/runtime/commitment. A returned fetch commitment must match before the local cache entry is marked verified.

MoE fragmentation requires the authoritative model commitment and, for MoE, the exact registered expert-manifest root/model/version/architecture.

Golden vectors pinned in Phase 164:

- expert manifest root: `d95cbef3e610fe781942580b477f00c6718806557090975548d33d103ef749ae`
- MoE fragment plan commitment: `8f595f57d7f5a0984a8ac713a8f7e194fb6135dbe3e50024f807df3d203a7a52`

The generic QRX core defines deterministic expert ranges but does not claim generic hidden-state/tensor aggregation for arbitrary models.

## Autonomous service audit

Phase 164 starts **two real background provider listeners** on OS-assigned localhost TCP ports. Both accept encrypted remote fragment execution and produce provider-signed encrypted responses through the actual remote server path.

The service also creates signed pod advertisements through its configured advertisement callback and recovers durable leases before entering service.

This proves same-host multi-service execution, not WAN/NAT/Internet-scale performance.

## DeepSeek boundary

The synthetic test model is named `qrx/deepseek-moe-reasoning` with architecture `deepseek-moe-compatible` to prove the model-aware expert-range scheduling path. There is **no vendor-specific DeepSeek consensus branch** and the release does not bundle DeepSeek weights or assert a license for an unspecified DeepSeek release. Real model licensing remains per exact registered model/version metadata.

## PQC validation boundary

Developer/container builds use `QRX_REQUIRE_PQC=OFF`. This is not a Mainnet production-PQC release certification. Existing PQ consensus/provider identity work remains separate from the optional classical X25519 convenience helper used in Phase 164.

## Packaging acceptance gates

The canonical full-tree package is accepted only after:

- zero predecessor files removed;
- complete root + GUI Wallet tree retained;
- no generated build directory in archive;
- 0.0.9.39 implementation/header/test present;
- implementation notes and release audit present at root and qrx-core level;
- clean build log present;
- 97/97 CTest log present;
- roadmap updated to 0.0.9.39 DONE;
- SHA-256 manifest verification passes;
- `unzip -t` archive integrity passes.

## Next phase

`0.0.9.40 – AURA Provider Auto-Bootstrap, PQ-Hybrid Sessions, Runtime Plugin/Model Loader & Distributed MoE Aggregation`
