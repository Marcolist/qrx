# QRX 0.0.9.63 — Upscaler Bundle + Header UI Hardening

- Normal/distribution builds now fail closed if the verified local AI bundle cannot be staged. Only explicit `QRX_ALLOW_AI_PENDING=1` permits a developer build without AI.
- Added `verify-upscaler-ai-bundle.sh`: runtime hash, both model manifests, param/bin hashes, provenance and required files are verified before Tauri packaging.
- macOS builds additionally verify that the complete AI resource tree exists inside the final `GUI Wallet.app` after Tauri bundling.
- Fixed GUI header collision: global top-tools get a dedicated responsive lane and hero/AURA/node actions wrap below it instead of overlapping.
- Packaged runtime/model discovery remains wired through Tauri resource paths; user Application Support remains a supported local override/update location.
