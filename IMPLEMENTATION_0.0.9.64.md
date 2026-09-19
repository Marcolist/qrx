# QRX 0.0.9.64 — Deterministic AI Asset Fetch & Diagnostics

- One canonical immutable Real-ESRGAN v0.2.5.0 release URL per asset.
- Exactly one network download per asset; verified cache is reused for model/runtime staging.
- No GitHub API digest discovery and no second fetch after a missing lock.
- Missing lock remains fail-closed (no TOFU), but the downloaded canonical artifact is retained in cache for independent review.
- Failure diagnostics print asset, canonical URL, byte count, actual SHA-256, expected SHA-256 and cache path.
- ZIP traversal validation happens before an archive can be staged.
- Existing reviewed Ubuntu lock remains unchanged. macOS/Windows locks are intentionally not invented.
