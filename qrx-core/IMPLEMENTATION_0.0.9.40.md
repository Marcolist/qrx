# QRX 0.0.9.40 – AURA Provider Auto-Bootstrap, PQ-Hybrid Sessions, Runtime Plugin/Model Loader & Distributed MoE Aggregation

Date: 2026-09-11
Status: Implemented and validated in developer/test mode

## Goal

Close the major post-0.0.9.39 provider-runtime gaps without hard-coding a specific AI vendor or runtime into QRX:

1. turn an explicitly opted-in calibrated host into a serving AURA provider with one bootstrap API;
2. replace the classical-only session bootstrap path with a post-quantum hybrid KEM path while keeping the existing AES-256-GCM payload channel;
3. load real inference runtimes through a stable plugin ABI and cache verified model handles;
4. add deterministic distributed MoE contribution/result encoding and aggregation instead of leaving the final cross-pod merge entirely undefined.

DeepSeek remains a normal model/profile family. The same plugin, cache, fragment and aggregation contracts apply to Qwen/Kimi/DeepSeek-compatible and future registered model families.

## Provider auto-bootstrap

New module:

- `qrx-core/src/compute/qrx_aura_provider_bootstrap.h`
- `qrx-core/src/compute/qrx_aura_provider_bootstrap.c`

`qrx_aura_provider_bootstrap_start()` accepts a calibrated host/provider configuration and performs the core provider startup sequence:

- requires explicit `user_enabled` opt-in;
- derives a `QrxResourceProviderPlan` for Compute + AI + Model Cache + Network;
- initializes the provider runtime and transitions it through READY to SERVING;
- optionally requires the existing wallet approval object for market registration;
- binds provider/pod identity and calibrated runtime device;
- opens the configured runtime plugin or validates a supplied fallback worker adapter;
- derives the advertised AURA pod capacity from the measured calibration profile;
- initializes the verified model-cache catalog;
- starts the autonomous 0.0.9.39 provider service;
- optionally auto-advertises the pod through the configured advertisement callback;
- enables secure dispatch and PQ-hybrid session negotiation when requested.

Bootstrap shutdown stops the provider service, unloads plugin models, drains the provider runtime and shuts it down when no jobs remain.

This is the core auto-bootstrap API. It does not yet mean that the GUI Wallet has a finished persisted “Provide AURA Compute” switch wired to every field of the bootstrap configuration.

## PQ-hybrid remote sessions

The 0.0.9.39 AES-256-GCM secure payload envelope remains unchanged as the confidentiality/data channel. 0.0.9.40 adds automatic KEM-based session establishment using the OpenSSL algorithm name:

`X25519MLKEM768`

New remote frame kinds:

- `SESSION_KEM_GET`
- `SESSION_OPEN`

New APIs include:

- `qrx_aura_pq_hybrid_kem_generate()`
- `qrx_aura_pq_hybrid_encapsulate()`
- `qrx_aura_pq_hybrid_decapsulate()`
- `qrx_aura_pq_session_cache_*()`
- `qrx_aura_remote_client_enable_peer_pq_hybrid()`
- `qrx_aura_remote_client_set_peer_hybrid_kem_public()`

The requester obtains the provider's hybrid KEM public key through the already signed/authenticated AURA remote frame protocol. The wire path transports the raw public key bytes and reconstructs the key using OpenSSL's provider-based raw-key API.

The requester encapsulates to the provider key, the provider decapsulates, and both derive the existing AURA secure-session object from the shared secret plus requester/provider identity. Dispatch then uses the existing AES-256-GCM envelope.

Provider-side PQ sessions are held in a bounded cache with creation/expiry heights and explicit secret cleansing on replacement/pruning/free.

### PQ compatibility boundary

The release container uses OpenSSL 3.5.5 and exposes `X25519MLKEM768` in the default provider. QRX therefore validates this concrete hybrid KEM path in that environment.

A production build must use an OpenSSL/provider configuration that actually exposes the same KEM algorithm. `QRX_REQUIRE_PQC=OFF` was used for the general developer build, so this release is not presented as an independent Mainnet PQC certification of the entire QRX tree.

The older X25519 helper remains as a compatibility/test bootstrap API and is still classical; the new automatically negotiated AURA session path can use `X25519MLKEM768`.

## Runtime plugin / model loader

New module:

- `qrx-core/src/compute/qrx_aura_runtime_plugin.h`
- `qrx-core/src/compute/qrx_aura_runtime_plugin.c`

The stable C ABI entry point is:

`qrx_aura_runtime_plugin_v1`

A plugin declares:

- ABI version;
- plugin name;
- QRX runtime ID;
- backend class;
- supported kernel mask;
- optional flags such as model-cache requirement and MoE-fragment input;
- optional device probe;
- model load callback;
- execute callback;
- model unload callback.

The host rejects ABI/backend/runtime mismatches. If a plugin declares that a model cache is required, the authoritative model manifest root must already exist in the supplied QRX Storage FS before load.

Loaded models are cached by authoritative `QrxAiModelRecord` commitment. A repeated execution of the same registered model reuses the existing model handle rather than loading it again. Closing the plugin host unloads all cached handles before unloading the dynamic library.

The implementation supports POSIX `dlopen`/`dlsym` and Windows `LoadLibrary`/`GetProcAddress` abstractions.

0.0.9.40 includes a test runtime plugin to validate the ABI and real dynamic-loader path. It does not bundle production llama.cpp, MLX, CUDA or other third-party inference engines.

## Distributed MoE aggregation

New module:

- `qrx-core/src/compute/qrx_aura_moe_aggregate.h`
- `qrx-core/src/compute/qrx_aura_moe_aggregate.c`

The 0.0.9.39 model-aware fragment plan already determines which admitted pod/expert range owns a fragment. 0.0.9.40 adds a deterministic output contract for fragment contributions and the final aggregate.

### Contribution envelope

Magic:

`QRXMC40`

Commitment domain:

`QRX/AURA/MOE-CONTRIBUTION/V1`

A contribution binds:

- fragment-plan commitment;
- fragment index/count;
- token index;
- vector length;
- fixed-point fractional-bit count;
- signed 32-bit weighted vector contribution.

### Aggregate envelope

Magic:

`QRXMR40`

Commitment domain:

`QRX/AURA/MOE-AGGREGATE/V1`

The aggregate requires exactly one contribution for every fragment index and rejects duplicate/missing/incompatible fragment metadata. Vectors are summed into signed 64-bit values with explicit overflow rejection.

The result is deterministic and independent of the order in which fragment responses arrive.

Phase 165 pins the aggregate result commitment:

`56ba51171fc9c0ec708135ade6da301f4c23226c45caccaa4ee592254aeb4f75`

### Model-semantics boundary

This is a deterministic fixed-point aggregation primitive, not a claim that all MoE architectures reduce to vector addition at every layer. Production runtime plugins remain responsible for generating semantically correct weighted contributions for their architecture. The generic QRX layer supplies the deterministic transport/fragment/aggregate contract and rejects malformed or inconsistent contributions.

## Provider-service integration

The autonomous provider service now supports:

- optional generated or supplied `X25519MLKEM768` private KEM key;
- PQ session cache;
- configurable session TTL with a bounded maximum;
- signed `SESSION_KEM_GET` response;
- signed `SESSION_OPEN` handshake;
- secure-session lookup from the PQ session cache;
- runtime-plugin execution callback in place of only the legacy worker adapter.

When secure dispatch is required, a normal remote dispatch still fails closed unless a valid secure session exists.

## Phase 165 validation

New test:

`compute_phase165_aura_autobootstrap_pq_runtime_moe`

The test verifies:

### PQ-hybrid KEM

- real `X25519MLKEM768` key generation through OpenSSL;
- encapsulation and decapsulation derive the same AURA session ID/key;
- modified KEM ciphertext does not produce an accepted equivalent session.

### Runtime plugin loader

- a real shared-library test plugin is loaded through the ABI;
- backend/runtime/device compatibility is checked;
- the registered model is loaded from the QRX Storage FS/cache;
- repeated execution reuses one cached model handle.

### Auto-bootstrap

- bootstrap is rejected when `user_enabled=0`;
- two independent providers are auto-started with real runtime state;
- both enter SERVING status;
- pod capacity is produced from the calibration profile;
- provider service endpoints are created;
- automatic advertisement callback is invoked;
- both providers report the hybrid KEM name.

### Secure distributed execution

- two provider services run on real localhost TCP endpoints;
- the client enables automatic PQ-hybrid session negotiation for both peers;
- secure sessions are negotiated automatically before dispatch;
- a registered synthetic DeepSeek-compatible MoE model is fragmented across the two admitted pods;
- both providers fetch/cache the model exactly once in the test path;
- fragment outputs are decoded and deterministically aggregated;
- final aggregate values are verified;
- leases/resources are released after execution;
- each provider holds one live PQ session during the test.

## Build/test validation

Final developer configuration:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

The final clean build used `--clean-first`. The first command invocation reached roughly 55% before the tool execution timeout; the same clean build directory was resumed without reconfiguration and reached 100%.

The complete registered suite was then rerun:

- **98 tests registered**
- **98 passed**
- **0 failed**

Compiler warning audit:

- canonical 0.0.9.39 clean-build warning lines: **413**
- final 0.0.9.40 clean-build warning lines: **413**
- new warning instances relative to 0.0.9.39: **0**
- compiler errors: **0**

The historical QRX tree is therefore not claimed warning-free.

## Scope boundary

0.0.9.40 does not yet provide:

- a finished GUI Wallet configuration flow that persists and auto-starts the provider bootstrap from a user-facing toggle;
- bundled production llama.cpp/MLX/CUDA runtime plugins;
- durable persistence of PQ session keys across provider restart (sessions are intentionally renegotiated after restart in this core path);
- a durable full remote job/result journal beyond the already durable lease/replay state;
- NAT traversal/relay orchestration for providers that cannot accept inbound TCP directly;
- architecture-specific multi-layer hidden-state/router/attention exchange for every MoE model;
- Internet-scale performance/convergence certification.

## Next

`0.0.9.41 – AURA Host/Wallet Provider Integration, Production Runtime Adapters, Durable Job/Result Journal & NAT/Relay Transport`
