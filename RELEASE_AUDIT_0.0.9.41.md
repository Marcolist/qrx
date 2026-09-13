# QRX 0.0.9.41 Release Audit

Date: 2026-09-11

## Release subject

AURA Host/Wallet Provider Integration, Production Runtime Adapters, Durable Job/Result Journal & NAT/Relay Transport.

## Security / correctness findings

### Durable retry ordering

The provider persists a semantic PREPARED job record before consuming the durable lease sequence. This closes the crash window where a sequence could be consumed without enough persistent information to distinguish a legitimate retry from a replay. COMMITTED results are replayable only for the same semantic request commitment; mutation under the same requester/lease/sequence is rejected.

### Relay trust boundary

The relay is a byte-forwarding transport and is not an AURA security endpoint. Request frames and provider responses remain signed end to end. PQ-hybrid KEM/session establishment and AES-256-GCM application payloads pass through the relay without relay-side key termination.

### Host configuration

The Core host validates versioned settings, caps, required identifiers, adapter kind and relay syntax. The GUI Wallet writes the same QRXAURA41 format and forces wallet approval, secure dispatch and PQ-hybrid sessions in its host profile.

### Runtime adapters

The new adapter layer resolves deployment plugins by backend/profile and retains the 0.0.9.40 ABI checks. It does not silently substitute an incompatible CPU/CUDA/MLX backend.

## Regression gate

New test: `compute_phase166_aura_host_runtime_journal_relay`.

Focused compatibility gate:

- Phase 164 autonomous secure provider service: PASS
- Phase 165 auto-bootstrap/PQ/runtime/MoE: PASS
- Phase 166 host/runtime/journal/relay: PASS

The full-suite count is expected to be **99** registered tests after Phase 166. The canonical package is accepted only after the final all-target build and 99/99 CTest run are recorded in the release logs.

## GUI validation boundary

The inline Wallet JavaScript passes `node --check` after extraction from `GUIWALLET/src/index.html`.

The current build container does not contain the Rust/Cargo toolchain, so a local `cargo check` for `GUIWALLET/src-tauri` cannot be executed in this environment. The release audit must state this limitation rather than report an unperformed Rust build.

## Known boundaries

- Production third-party inference engine binaries/libraries are deployment dependencies, not vendored artifacts.
- The wallet-to-daemon config path handoff is present; a richer long-running provider supervisor/reload/health layer follows in 0.0.9.42.
- One relay endpoint is configured per provider in 0.0.9.41; failover follows.
- Job journal retention/compaction follows.
- PQ session secrets are intentionally renegotiated after restart in this phase.

## Next

`0.0.9.42 – AURA Provider Supervisor, Multi-Relay Failover, Journal Retention & Runtime Health`
