# QRX Core 0.0.9.41 – AURA Host/Wallet Provider Integration, Production Runtime Adapters, Durable Job/Result Journal & NAT/Relay Transport

Date: 2026-09-11

## Objective

0.0.9.41 closes the four production gaps explicitly left by 0.0.9.40: a persistent Wallet/Core provider-host contract, stable production runtime-adapter selection, crash-durable remote job/result semantics, and provider-initiated NAT/relay transport.

## AURA provider host + GUI Wallet

New core host API:

- `src/compute/qrx_aura_provider_host.h`
- `src/compute/qrx_aura_provider_host.c`

The host consumes a versioned text configuration (`format=QRXAURA41`) and composes the existing calibrated provider bootstrap rather than creating a second provider implementation. Settings include provider/pod/network/region, compute/cache/network caps, wallet approval, secure/PQ requirements, runtime-adapter selection, local listener, lease/job journals and optional relay endpoint.

The GUI Wallet Resource Globe now has a durable **AURA Host** section instead of storing the compute-provider intent only in browser `localStorage`. Tauri commands save/load the same QRXAURA41 contract under the wallet/network settings directory and the daemon spawn path receives `QRX_AURA_PROVIDER_CONFIG` for host/supervisor handoff.

Safety remains fail-closed:

- provider enablement is explicit;
- wallet approval stays mandatory in the wallet profile;
- secure remote dispatch is forced on;
- PQ-hybrid session negotiation is forced on;
- the wallet does not sign an on-chain transaction merely by saving provider settings.

## Production runtime adapter profiles

New adapter resolver:

- `src/compute/qrx_aura_runtime_adapters.h`
- `src/compute/qrx_aura_runtime_adapters.c`

Profiles:

- `AUTO`
- `LLAMA_CPP_CPU`
- `LLAMA_CPP_CUDA`
- `MLX_METAL`
- `EXTERNAL`

Resolution order is explicit plugin path, backend-specific environment override, configured adapter directory, then canonical platform filename. Backend compatibility is checked before loading through the stable 0.0.9.40 runtime-plugin ABI.

Canonical deployment filenames are platform-specific (`.dll`, `.dylib`, `.so`). Third-party inference engines are intentionally not vendored into QRX Core; this phase supplies the stable production host/adapter contract and deployment resolver.

## Durable semantic job/result journal

New journal:

- `src/compute/qrx_aura_job_journal.h`
- `src/compute/qrx_aura_job_journal.c`

On-disk magic: `QRXAJR41`

Domains:

- `QRX/AURA/JOB-RESULT-JOURNAL/V1`
- `QRX/AURA/JOB-REQUEST/V1`
- `QRX/AURA/JOB-RESULT/V1`

A dispatch now follows the durable order:

1. decrypt/verify request;
2. PREPARE semantic job record and fsync/replace journal;
3. commit lease sequence barrier and durable lease journal;
4. execute runtime;
5. COMMIT result bytes/status and fsync/replace job journal;
6. return the signed/encrypted response.

The semantic request commitment excludes the transient request height and transport encryption nonce, but binds all execution-relevant fields plus the post-decryption payload commitment.

Consequences:

- exact PREPARED retry after a crash may continue execution even though the lease sequence was already committed;
- exact COMMITTED retry returns the persisted result without executing the runtime again;
- a changed request using the same requester/lease/sequence is rejected;
- job/result persistence is separate from, and complementary to, the existing durable lease journal.

## NAT / blind relay transport

New relay module:

- `src/compute/qrx_aura_relay.h`
- `src/compute/qrx_aura_relay.c`

Endpoint syntax:

`qrxrelay://host:port/route-id`

A provider opens the connection outbound and registers its route. Requesters connect to the same route for each framed QRX request/response. The relay forwards framed bytes and does not terminate AURA signatures, KEM negotiation or AES payload protection.

Provider-service integration:

- optional relay endpoint + `relay_required` fail-closed startup policy;
- provider advertisement publishes the relay endpoint when configured;
- persistent provider tunnel reconnect loop;
- local listener remains available for same-host/LAN/direct deployments;
- relay and direct listener share the same lease/job/PQ state under a service state lock;
- shutdown performs socket `shutdown()` before close so blocked relay receives terminate cleanly.

## Regression validation

New registered test:

`compute_phase166_aura_host_runtime_journal_relay`

It validates:

- QRXAURA41 host settings save/load;
- MSVC-portable numeric config parsing;
- production CPU adapter resolution/open through the stable runtime ABI;
- durable PREPARED -> COMMITTED journal save/load;
- exact committed retry recognition and rejection of a mutated same-sequence request;
- two real provider services registered through provider-initiated relay tunnels;
- automatic PQ-hybrid secure-session negotiation through the blind relay;
- distributed two-provider MoE execution over `qrxrelay://`;
- persisted COMMITTED job results for both providers;
- clean provider/relay shutdown.

Compatibility targets 164 and 165 remain green.

## Scope boundary

0.0.9.41 does not claim:

- that llama.cpp, CUDA or MLX themselves are bundled into the QRX source archive;
- universal architecture-independent tensor/attention exchange for arbitrary MoE models;
- multi-relay failover/health scoring;
- automatic journal retention/compaction beyond the bounded in-memory journal limit;
- durable PQ session secrets across restart (renegotiation remains the safer default in this phase);
- WAN/Internet-scale throughput certification.

## Next

`0.0.9.42 – AURA Provider Supervisor, Multi-Relay Failover, Journal Retention & Runtime Health`
