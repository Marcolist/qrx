# QRX 0.0.9.66 — Tauri AI Resource Packaging Hardening

Fixes the final application-bundle boundary for the verified local AI Upscaler.

Root cause: target-specific Tauri config overlays replace the base `bundle.resources` array. The macOS/Linux/Windows overlays listed the AURA and helper resources but omitted `resources/upscaler/**/*`, so staging passed while the generated application omitted the AI payload.

Changes:
- Adds `resources/upscaler/**/*` to every target-specific Tauri bundle resource list.
- Keeps pre-Tauri AI bundle verification mandatory for distribution builds.
- On macOS, verifies the packaged bundle and compares a canonical SHA-256 inventory of every staged AI file against the files embedded in `GUI Wallet.app`.
- A mismatch or missing packaged AI resource remains a hard release failure.
