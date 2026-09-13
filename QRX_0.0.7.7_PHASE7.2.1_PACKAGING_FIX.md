# QRX 0.0.7.7 Phase 7.2.1 — Packaging Fix

Phase 7.2.1 restores the unified release entry point that was accidentally omitted from the Phase 7.2 source ZIP.

## Restored / hardened

- `scripts/build-all-targets.sh`
- `scripts/build-all-targets.ps1`
- `GUIWALLET/src-tauri/build.rs` for the Tauri 1.x build context
- `.github/workflows/build-all-targets.yml` five-target native release matrix
- native-host guard: a release can no longer be mislabeled as another OS/architecture
- dependency order: Core/QRXDB -> CLI/tools -> BTC wallet service -> exact Tauri sidecars -> GUI/installers -> checksummed release ZIP
- macOS DMG creation uses `hdiutil` after the `.app` build and does not depend on Finder/AppleScript layout automation
- Bash 3.2-safe optional Cargo lock handling (no empty-array expansion)

The Genesis Governance / KYC Provider Registry consensus changes from Phase 7.2 are unchanged.
