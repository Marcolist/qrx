# QRX 0.0.9.44 — Provider Supervisor, Relay Failover State, Runtime Health & Journal Retention

Date: 2026-09-11
Status: implemented and regression-tested.

## Goal
Add the lifecycle controls required for unattended home/edge providers: health, relay choice/failover decisions, restart recommendation and bounded durable history.

## Core implementation
New module: `src/compute/qrx_aura_provider_supervisor.{h,c}`.

### Relay health/failover state
- up to four configured relay endpoints,
- success/failure/consecutive-failure counters,
- observed latency,
- deterministic active-relay selection,
- exponential cooldown after repeated failure,
- fail-closed restart recommendation when relay is required and no relay is currently usable.

The supervisor is intentionally transport-agnostic. 0.0.9.41 supplies the real outbound relay tunnel; the supervisor chooses/health-scores endpoints. Concurrent multi-relay tunnels and signed runtime-package activation are part of the following production-host phase.

### Runtime health
- runtime probe success/failure tracking,
- consecutive failure threshold,
- restart recommendation after repeated runtime failure,
- recovery clears the restart condition.

### Durable journal retention
- defaults retain the newest 128 committed and 32 prepared job records,
- compaction calls the 0.0.9.41 durable job/result journal,
- compacted state is persisted,
- result history therefore does not grow without bound on always-on providers.

## Validation
New test: `compute_phase169_aura_provider_supervisor_failover_health`.

Covers:
- relay failure/cooldown and lower-latency healthy relay selection,
- runtime failure threshold/recovery,
- committed/prepared journal retention and reload,
- supervisor status snapshot.

Full registered CTest: **102/102 PASS**.
GUI inline JavaScript syntax check: PASS.
Tauri Rust compile: not executed in this container because `cargo`/`rustc` are not installed here.
