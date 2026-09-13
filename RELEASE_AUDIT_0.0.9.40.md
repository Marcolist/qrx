# QRX 0.0.9.40 Release Audit

Date: 2026-09-11
Release: AURA Provider Auto-Bootstrap, PQ-Hybrid Sessions, Runtime Plugin/Model Loader & Distributed MoE Aggregation

## Canonical predecessor

Baseline archive:

`qrx-core-0.0.9.39-autonomous-provider-durable-secure-moe.zip`

The 0.0.9.40 tree was compared against a fresh extraction of that canonical predecessor with build directories excluded.

Pre-release implementation comparison before final manifest creation reports:

- predecessor regular files: **1140**
- removed predecessor files: **0**
- new core implementation/test files: **8**
- changed core implementation/build files: **5**

New core files:

- `qrx-core/src/compute/qrx_aura_provider_bootstrap.c`
- `qrx-core/src/compute/qrx_aura_provider_bootstrap.h`
- `qrx-core/src/compute/qrx_aura_runtime_plugin.c`
- `qrx-core/src/compute/qrx_aura_runtime_plugin.h`
- `qrx-core/src/compute/qrx_aura_moe_aggregate.c`
- `qrx-core/src/compute/qrx_aura_moe_aggregate.h`
- `qrx-core/tests/compute_phase165_aura_autobootstrap_pq_runtime_moe.c`
- `qrx-core/tests/qrx_aura_test_runtime_plugin.c`

Changed core files:

- `qrx-core/CMakeLists.txt`
- `qrx-core/src/compute/qrx_aura_provider_service.c`
- `qrx-core/src/compute/qrx_aura_provider_service.h`
- `qrx-core/src/compute/qrx_aura_remote_dispatch.c`
- `qrx-core/src/compute/qrx_aura_remote_dispatch.h`

Release documentation, logs and roadmap updates are additional release artifacts.

## CMake/test-registration cleanup

During release finishing an inherited registration typo was found: the Phase-164 `add_test()` line accidentally passed the Phase-165 executable name as an extra argument. The registration was corrected, and Phase 165 was added to the non-MSVC `-UNDEBUG` test-target list so its `assert()` checks remain active consistently with the prior compute tests.

Focused Phase 164/165 validation passed after that correction.

## Clean build audit

Developer/test configuration:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

A final `--clean-first` build was executed. The first command invocation reached roughly 55% before the tool execution timeout. The **same clean build directory** was resumed without reconfiguration and reached **100%**, including `qrx`, `qrxd`, `qrx-cli`, wallet CLI, QRXDB tools, the test runtime shared library and all configured test executables.

The release therefore records a completed resumed clean build rather than claiming that a single tool invocation ran uninterrupted.

Compiler warning comparison:

- canonical 0.0.9.39 clean-build warning lines: **413**
- final 0.0.9.40 clean-build warning lines: **413**
- normalized new warning instances: **0**
- normalized removed warning instances: **0**
- compiler error lines: **0**

No warning line references the new 0.0.9.40 provider-bootstrap, runtime-plugin, MoE-aggregate or Phase-165 source paths.

The historical QRX tree is not claimed warning-free.

## Full test audit

After the final clean build, the complete registered CTest suite was rerun:

- registered tests: **98**
- passed: **98**
- failed: **0**

Focused compatibility tests:

- Phase 164 autonomous provider service / secure durable MoE: PASS
- Phase 165 auto-bootstrap / PQ-hybrid / runtime plugin / MoE aggregate: PASS

## PQ-hybrid audit

The container environment reports OpenSSL 3.5.5 and exposes the provider algorithm:

`X25519MLKEM768`

The 0.0.9.40 remote-session code uses OpenSSL provider APIs for real KEM key generation, encapsulation and decapsulation. The provider's hybrid public key is transferred as raw public-key bytes inside the signed AURA remote frame protocol and reconstructed with the OpenSSL provider raw-key API.

Phase 165 validates:

- real hybrid key generation;
- requester encapsulation;
- provider decapsulation;
- equality of the derived AURA session key/session ID;
- modified ciphertext does not yield an accepted equivalent session.

The KEM-established secret feeds the existing AES-256-GCM AURA secure payload channel. Existing remote-frame signatures remain the authentication/integrity authority for the handshake metadata and provider identity.

### PQC boundary

The full developer build was configured with `QRX_REQUIRE_PQC=OFF`; this is not a Mainnet-wide PQC certification. The concrete AURA KEM test itself nevertheless uses the actual `X25519MLKEM768` OpenSSL implementation available in the container.

Production deployments need an OpenSSL/provider build that exposes this algorithm.

## Provider bootstrap audit

The new bootstrap API is explicitly opt-in (`user_enabled`). It creates the resource-provider plan from measured calibration and configured host limits, initializes/starts the provider runtime, optionally enforces wallet approval for market registration, opens a runtime plugin or fallback adapter, derives advertised AURA capacity, and starts the autonomous provider service.

Phase 165 verifies rejection when opt-in is disabled and successful startup of two independent providers in SERVING state with automatic advertisement callbacks.

This is the core bootstrap primitive; it is not yet claimed as a finished persistent GUI Wallet settings workflow.

## Runtime plugin/model loader audit

The stable C ABI entry symbol is:

`qrx_aura_runtime_plugin_v1`

The host validates ABI version, backend/runtime compatibility and optional device probe. Plugins may require the registered model manifest root to exist in QRX Storage FS before load.

Loaded model handles are keyed by authoritative model commitment and reused for repeated execution. Plugin close unloads all cached model handles and then the dynamic library.

Phase 165 builds and loads a real shared-library test plugin. This validates the loader ABI, not a bundled production llama.cpp/MLX/CUDA integration.

## Distributed MoE aggregation audit

0.0.9.40 defines deterministic contribution/result envelopes and commitment domains:

- contribution magic: `QRXMC40`
- result magic: `QRXMR40`
- `QRX/AURA/MOE-CONTRIBUTION/V1`
- `QRX/AURA/MOE-AGGREGATE/V1`

Aggregation requires one unique contribution per fragment index with the same fragment plan, token, vector length and fixed-point precision. Signed 32-bit fragment values are summed into signed 64-bit results with explicit overflow rejection.

The same result is obtained independent of fragment-arrival order; duplicate fragment indexes are rejected.

Phase-165 golden aggregate commitment:

`56ba51171fc9c0ec708135ade6da301f4c23226c45caccaa4ee592254aeb4f75`

The generic QRX aggregate is a deterministic fixed-point primitive. The release does not claim that arbitrary MoE hidden-state/attention/router semantics can always be represented by simple vector addition; production runtime plugins remain responsible for producing the correct weighted contribution contract for their architecture.

## End-to-end Phase-165 path

The regression starts two real provider services and validates:

- auto-bootstrap from two calibrated provider configs;
- real dynamic runtime plugin loading;
- model-cache-backed model handle creation/reuse;
- automatic `X25519MLKEM768` secure-session negotiation for both remote peers;
- real localhost TCP remote dispatch;
- synthetic DeepSeek-compatible MoE expert fragmentation from the existing plan layer;
- per-provider model fetch/cache path;
- deterministic two-fragment aggregate values/result commitment;
- resource/lease cleanup after dispatch.

DeepSeek remains a model/profile family and is not hard-coded as a consensus rule.

## Known boundaries

0.0.9.40 does not yet include:

- finished persistent GUI Wallet controls for provider bootstrap;
- bundled production llama.cpp/MLX/CUDA runtime plugins;
- durable PQ session-key persistence across restart;
- durable full remote job/result journal beyond lease/replay persistence;
- NAT traversal/relay transport for unreachable providers;
- universal model-specific multi-layer tensor/attention exchange;
- WAN/Internet-scale performance certification.

## Packaging acceptance gates

The canonical 0.0.9.40 full-tree package is accepted only after:

- zero predecessor files removed;
- complete root + GUI Wallet tree retained;
- no generated `build` directory in archive;
- all eight new core source/test/plugin-test files present;
- root + qrx-core implementation and release audit present;
- final resumed-clean-build log present;
- 98/98 CTest log present;
- roadmap updated to 0.0.9.40 DONE;
- SHA-256 full-tree manifest verifies;
- `unzip -t` archive integrity passes.

## Next

`0.0.9.41 – AURA Host/Wallet Provider Integration, Production Runtime Adapters, Durable Job/Result Journal & NAT/Relay Transport`
