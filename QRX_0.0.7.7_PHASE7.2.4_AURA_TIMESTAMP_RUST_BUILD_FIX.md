# QRX 0.0.7.7 Phase 7.2.4 — AURA Timestamp Rust Build Fix

Fixes the Tauri/Rust compile error:

`error[E0425]: cannot find function chrono_like_timestamp in this scope`

## Change

Adds the missing dependency-free helper in `GUIWALLET/src-tauri/src/main.rs`:

- uses `SystemTime` / `UNIX_EPOCH` already imported by the wallet
- returns Unix epoch milliseconds as `u128`
- requires no new Rust crate
- preserves the existing AURA local JSONL history format semantics (`ts`, `role`, `content`)

No consensus, Genesis, privacy, tokenization, KYC, VELOCITY or Generals logic was changed.
