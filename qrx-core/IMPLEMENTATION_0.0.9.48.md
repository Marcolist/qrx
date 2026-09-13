# QRX 0.0.9.48 — Mainnet AURA Release Hardening, Migration, Packaging & RC Readiness

Implemented:
- `qrx_aura_release.{h,c}`.
- idempotent AURA host-config migration/hardening for secure dispatch, PQ sessions, runtime AUTO state, journals and conservative cache budget.
- release-readiness gate checks host security, native hardware/runtime package readiness, governed recommended models and distribution/bootstrap availability.
- deterministic readiness commitment.
- governance BLOCK immediately invalidates AURA release readiness.
- phase173 regression test.

Validation after 0.0.9.48: **107/107 CTest tests PASS (including the phase174 documentation closeout gate)**.
Native Tauri installers still require their native Rust/Tauri build runners and are not claimed validated by the Linux C container alone.
