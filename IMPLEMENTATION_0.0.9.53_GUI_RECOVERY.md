# QRX 0.0.9.53 — GUI Recovery, External Apps & Browser UX

This source snapshot follows 0.0.9.52 and keeps the hermetic native dependency and Tauri compile fixes intact.

## Fixed

- Removed the stray literal `html` before the wallet document root.
- Reworked localization application so translated containers no longer destroy nested form controls or nested translation spans via `textContent` replacement.
- Replaced the fragmented splash title with a stable, non-fragmented `QRX CHAIN` identity.
- Splash now shows `QUBITCOIN · QUB` and the canonical Genesis memo below it.
- Added responsive layout hardening for QRX-Net, Resource Globe, dashboard metrics and narrow desktop/window sizes.
- Moved top-level language/experience controls out of the fixed scrolling overlay behavior.
- QRX Generals remains an external Tauri application window.
- QRX Browser opens as its own Tauri window and now has browser-style chrome: tab strip, back/forward/reload/home, address bar, QRX/WWW route status, content viewport and a separate Safety drawer.
- QRX Upscaler now opens directly in its own Tauri application window from the Apps launcher.
- Added the `open_qrx_upscaler_window` Tauri command and standalone Upscaler shell.
- Browser CSP now explicitly permits HTTP/HTTPS frame routes for the isolated WWW viewport; QRX content continues to use verified QRX-Net fetch + srcdoc rendering.

## Security boundaries retained

- QRX `.qrx` resolution never falls back to DNS.
- QRX-Net fetch remains daemon-backed and verified before rendering.
- Family Safety policy remains local and encrypted at rest through the existing wallet command path.
- Upscaler distributed compute remains gated behind `COMPUTE_POUC_V1` and requires explicit opt-in.
- No consensus, Genesis, staking, wallet-key, tokenomics or governance rules were changed in this GUI pass.

## Validation performed in packaging environment

- `tauri.conf.json` JSON parse: PASS
- `scripts/build-all-targets.sh` shell syntax: PASS
- `qrx-core/scripts/build-macos-static.sh` shell syntax: PASS
- HTML parser: wallet/browser/upscaler/generals PASS
- JavaScript syntax (`node --check`): wallet/browser/upscaler PASS

A native Tauri/Rust build must still be run on the target macOS/Windows/Linux host.
