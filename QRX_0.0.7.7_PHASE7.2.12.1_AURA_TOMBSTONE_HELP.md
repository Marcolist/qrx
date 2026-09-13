# QRX 0.0.7.7 Phase 7.2.12.1 — AURA Tombstone Help

AURA Local Help now explains validator status terms in plain language.

- `jailed` = temporarily excluded from active consensus.
- `tombstoned` = permanently excluded after a provable severe consensus violation, especially double-signing/equivocation.
- Default QRX double-sign slash remains 5000 bps (50% of slashable stake).
- Offline status alone does not cause a tombstone.
- A tombstoned identity cannot be restored merely by toggling Validator Mode back on.

The answer is available both through the Rust-backed AURA local-help command and the wallet's immediate local AURA UI fallback.
