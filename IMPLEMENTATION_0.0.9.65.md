# QRX 0.0.9.66 — Supply-Chain Locked AI Packaging

- Promoted the deterministic macOS Real-ESRGAN v0.2.5.0 archive digest to a committed QRX release lock.
- Pinned macOS archive size in addition to SHA-256.
- Added offline verified-cache mode (`QRX_AI_OFFLINE=1`).
- Retained fail-closed behavior for unknown/mismatching target artifacts.
- No GitHub API digest discovery and no automatic TOFU lock promotion.
- Added explicit supply-chain policy and provenance marker `qrx-0.0.9.66-sha256-release-lock`.
- Final Tauri application resource verification from 0.0.9.63 remains mandatory.
