# QRX 0.0.9.39 – Autonomous AURA Provider Service, Durable Lease Journal, Secure Remote Payload Channel & Model-Aware MoE Fragmentation

Date: 2026-09-10
Status: Implemented and validated in developer/test mode

## Goal

Close the four explicit 0.0.9.38 execution gaps without pretending that a generic blockchain core understands the hidden-state/tensor semantics of every AI model:

1. provide a real long-running AURA provider service abstraction rather than only a request/response helper;
2. make remote reservation leases recoverable after a provider process restart;
3. add application-layer confidentiality on top of the already signed/authenticated remote frames;
4. define a deterministic model-aware MoE fragmentation contract that can partition registered experts across admitted pods while leaving model-specific aggregation to the runtime adapter.

DeepSeek remains a normal registered AURA model family/profile. There is no DeepSeek consensus branch. The Phase-164 synthetic model is deliberately `deepseek-moe-compatible` to validate the distributed expert-range path used by reasoning-oriented MoE profiles.

## New autonomous provider service

New module:

- `qrx-core/src/compute/qrx_aura_provider_service.h`
- `qrx-core/src/compute/qrx_aura_provider_service.c`

The provider service owns a real TCP listener and background service loop once started through `qrx_aura_provider_service_start()`.

A service instance binds:

- one `QrxAuraRuntimeBinding` / provider runtime;
- model-cache catalog;
- authoritative AI model registry;
- requester public-key lookup;
- provider private signing key;
- chain-height callback;
- secure-session lookup;
- model-fetch callback;
- durable lease-journal path;
- listening host/port;
- optional automatic signed pod advertisement.

Port `0` requests an OS-assigned listener port. The service exposes its resulting `qrxp2p://host:port` endpoint, active lease count, selected port and announcement sequence.

The background loop accepts remote AURA frames and routes them through the hardened 0.0.9.38 remote server path. Advertisement refresh occurs before the configured height-based TTL expires.

The service API has Linux/POSIX and Windows listener/thread implementations. The release regression exercises the POSIX loopback path in the container.

### Scope boundary

0.0.9.39 provides the autonomous service runtime/API once configured and started by a host process. It is not yet automatically instantiated from QRX Wallet/node configuration. Node/wallet auto-bootstrap is intentionally left for the next phase.

## Durable reservation lease journal

The remote-dispatch layer now exposes:

- `qrx_aura_reservation_leases_journal_save()`
- `qrx_aura_reservation_leases_journal_load()`
- `qrx_aura_reservation_leases_journal_recover()`
- `qrx_aura_reservation_leases_reap_durable()`

Journal identity:

- magic: `QRXALJ39`
- commitment/checksum domain: `QRX/AURA/LEASE-JOURNAL/V1`
- format version: 1

The journal serializes lease state with explicit big-endian integer encoding. Save uses a temporary file, flush + `fsync`/platform equivalent, and atomic replace/rename.

Load verifies:

- magic/version framing;
- bounded payload size;
- exact EOF/no trailing bytes;
- SHA3-256 domain-separated checksum;
- decoded lease invariants.

Recovery against fresh provider runtimes:

- expired ACTIVE leases become EXPIRED;
- still-live ACTIVE leases re-reserve their compute-thread/network resources;
- if any resource restoration fails, all reservations restored during that recovery attempt are rolled back;
- a modified recovered table is persisted again before successful return.

Durable reap similarly couples resource release to journal persistence and attempts to restore runtime/table state if persistence fails.

The server also journals the accepted DISPATCH sequence barrier before worker execution. This prevents a provider crash after execution from simply making an already accepted sequence replayable after restart.

## Secure remote payload channel

The existing signed 0.0.9.38 remote frame remains the identity and integrity authority. 0.0.9.39 adds an encrypted payload envelope for confidentiality.

Secure-channel constants:

- channel version 1;
- AES-256-GCM;
- 32-byte session key;
- 12-byte random GCM nonce;
- 16-byte authentication tag;
- envelope magic `QRXSEC39`.

New APIs include:

- `qrx_aura_secure_session_from_secret()`
- `qrx_aura_secure_session_x25519()`
- `qrx_aura_secure_payload_is_envelope()`
- `qrx_aura_secure_payload_seal()`
- `qrx_aura_secure_payload_open()`
- `qrx_aura_remote_client_set_peer_secure_session()`

The AES-GCM AAD binds the remote-frame execution metadata, including requester/provider/pod identity, sequence, lease, admission, model/runtime, node and fragment coordinates. A successful provider response is encrypted before it is signed/sent when the request used a secure session.

Random nonces come from `RAND_bytes`; deterministic nonce derivation was intentionally removed to avoid accidental key/nonce reuse after a process/client sequence reset.

A provider may set `require_secure_dispatch`, causing plaintext DISPATCH payloads to fail closed.

### PQC boundary

`qrx_aura_secure_session_x25519()` is a classical bootstrap helper and is **not** claimed to be post-quantum. The secure-channel API is intentionally keyed by an abstract 32-byte session secret so a future ML-KEM or hybrid ML-KEM+X25519 establishment can feed the same AES-256-GCM channel without changing the dispatch envelope. Production QRX PQ policy is therefore not represented by the X25519 convenience helper alone.

## Automatic model fetch on provider cache miss

The provider-side remote server can now receive:

- authoritative `QrxAiModelRegistry`;
- `QrxAuraModelCacheFetchFn` callback and context.

Before execution, a cache miss no longer has to fail immediately. The provider verifies that model ID/version/runtime and model commitment match the authoritative registry record, derives a placement requirement for the local pod, calls the configured model-fetch adapter, and only marks the cache entry verified when the returned model commitment matches exactly.

For a real QRX Drive deployment the callback can be `qrx_aura_drive_model_fetch`, introduced in 0.0.9.38. Phase 164 uses a deterministic test fetch adapter so the autonomous secure service path can be tested without duplicating the full CAS/Drive regression already covered by Phase 163.

## Expert manifest

New `QrxAuraExpertManifest` binds:

- model ID/version;
- architecture;
- layer count;
- expert count;
- experts per token.

Expert-manifest object encoding is canonical and its object root is the plain SHA3-256 of the encoded bytes so it can directly correspond to a QRX Storage FS/CAS object ID and to `QrxAiModelRecord.expert_manifest_root`.

`qrx_aura_expert_manifest_load()` reads the manifest from QRX Storage FS and rejects any mismatch in object root, model ID, model version or architecture.

The Phase-164 golden expert-manifest root is:

`d95cbef3e610fe781942580b477f00c6718806557090975548d33d103ef749ae`

## Model-aware MoE fragment plan

0.0.9.39 adds `QrxAuraMoeFragmentPlan` and deterministic strategies:

- `SINGLE`
- `REPLICATED`
- `EXPERT_RANGE`

For a registered MoE model, the plan requires the exact authoritative model commitment and exact expert-manifest binding. Experts are divided deterministically into contiguous ranges across admitted pods.

Example used by Phase 164:

- 8 experts;
- 2 admitted pods;
- pod 0 receives experts 0..3;
- pod 1 receives experts 4..7;
- 24 layers;
- 2 experts/token metadata.

Fragment-plan commitment domain:

`QRX/AURA/MOE-FRAGMENT-PLAN/V1`

Phase-164 golden plan commitment:

`8f595f57d7f5a0984a8ac713a8f7e194fb6135dbe3e50024f807df3d203a7a52`

The fragment payload envelope (`QRXMF39`) carries the plan commitment, fragment index/count, expert range, layer range, experts-per-token and the original opaque input payload.

`qrx_aura_moe_fragment_input_adapter()` directly implements the 0.0.9.38 remote fragment-input callback contract.

### Important aggregation boundary

The generic QRX core still does **not** invent model-specific tensor, hidden-state, router-logit or attention aggregation. The fragmentation contract determines which model/expert slice belongs to which pod; the actual runtime/model adapter remains responsible for executing that slice and the caller/runtime still supplies the model-specific aggregation callback.

This is deliberate: a DeepSeek-compatible MoE, a Kimi-compatible MoE and another future architecture may require different execution/aggregation semantics even if QRX schedules them through the same provider/lease/PoUC infrastructure.

## Phase 164 validation

New test:

`compute_phase164_aura_provider_service_secure_durable_moe`

It validates three major paths.

### Secure channel

- two real X25519 keypairs derive the same test session;
- AES-256-GCM seals a private reasoning prompt;
- the plaintext byte sequence is not present in the encrypted envelope;
- the opposite endpoint decrypts the original bytes exactly;
- modified GCM authentication tag is rejected.

The test uses X25519 only as a convenient test key-establishment mechanism; this is not a PQC certification.

### Durable leases

- a real provider runtime receives a reservation;
- the lease table is journaled;
- a fresh runtime loads/recoveries the journal and re-reserves the live resources;
- release persists correctly;
- another fresh runtime sees no active reservation;
- a deliberately corrupted journal checksum is rejected.

### Autonomous secure two-provider MoE execution

- two real background AURA provider services bind OS-assigned loopback TCP ports;
- each service has a distinct provider Ed25519 key and secure requester/provider session;
- both auto-advertise signed pod announcements;
- a synthetic registered `qrx/deepseek-moe-reasoning` / `deepseek-moe-compatible` profile binds 8 experts across 2 pods;
- services begin with empty verified model caches;
- first dispatch auto-fetches the model through the configured provider fetch adapter;
- the deterministic fragment plan sends E0+4 to provider 1 and E4+4 to provider 2;
- both encrypted remote fragments are actually transported to the background TCP services;
- the provider worker decodes the model-aware fragment envelope and executes its fragment;
- the test aggregation receives both fragment outputs;
- provider caches become verified;
- remote leases are released and runtime reservations return to zero.

This is a real same-host loopback multi-service integration test. It is not an Internet/WAN throughput benchmark.

## Build and regression validation

Developer/container configuration:

- `QRX_BUILD_TESTS=ON`
- `QRX_REQUIRE_PQC=OFF`

The final `--clean-first` build was started from the configured 0.0.9.39 tree. The first execution reached approximately 57% before the tool execution limit interrupted it; the exact same clean build tree was resumed and completed to 100%. There was no compilation failure.

Final warning count: **413**, identical to the canonical 0.0.9.38 clean-build warning count. No warning line in the final log references the new 0.0.9.39 remote-dispatch/provider-service/Phase-164 files.

Final registered suite: **97/97 CTests PASS**.

`QRX_REQUIRE_PQC=OFF` remains only a container/developer validation configuration and does not relax QRX Mainnet PQ requirements.

## Remaining boundary / next phase

0.0.9.39 deliberately leaves the following for the next integration layer:

- automatic start/configuration of the AURA provider service from node/wallet configuration;
- PQ-hybrid session establishment (e.g. ML-KEM + classical hybrid) instead of relying on a caller-provided secret or optional X25519 helper;
- production model runtime/plugin loader that maps registry/cache objects to concrete llama.cpp/MLX/CUDA/etc execution backends;
- architecture-specific distributed hidden-state/expert aggregation for real large MoE models;
- durable remote job/result journal beyond the durable reservation/replay barrier;
- WAN-scale performance, NAT traversal and high-churn provider-service stress validation.

Next planned phase:

`0.0.9.40 – AURA Provider Auto-Bootstrap, PQ-Hybrid Sessions, Runtime Plugin/Model Loader & Distributed MoE Aggregation`
