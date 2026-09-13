# QRX 0.0.9 — Apple Metal Profiles + QRX App Foundation

## Upscaler capability profiles

- Apple Silicon is now split into M1, M2, M3 and M4 profiles.
- Apple Silicon reports native Metal availability separately from Vulkan compatibility.
- The current NCNN AI path on macOS uses MoltenVK; macOS is never labelled as native Vulkan.
- All Apple M generations now share the single `apple-silicon` compatibility profile; detected generation/variant is metadata used only for display and Auto-tuning hints.
- Raspberry Pi 5 remains supported through native V3DV Vulkan when a usable render device exists.
- ODROID-N2/N2+ remains experimental through Mali-G52/PanVK.
- Generic Linux/Windows differentiates native Vulkan, loader-only and unavailable states.
- Tile size remains memory-driven; chip generation adjusts scheduler/thread hints, not unsafe VRAM assumptions.

## QRX App Foundation 0.0.9

Implemented:

- `.qrxapp` ZIP container format v1
- bounded extraction (64 MiB package/expanded cap, 16 MiB per file, 256 files)
- path traversal and symlink rejection
- Wallet App Registry
- local sideload install/uninstall
- Developer Mode for live unpacked folders
- sandboxed QRX App Host with `allow-scripts` only and no same-origin
- restrictive per-app CSP with network/form/object access denied
- Rust permission re-check for every Mini SDK call
- Mini JS SDK v1
- app-scoped 64 KiB storage
- public identity/balance/height/network read bridge
- payment request bridge that only pre-fills the normal Wallet Send flow
- Hello QRX reference app + package builder + ready `.qrxapp`

Explicitly deferred to 0.0.10:

- developer signature verification / trust roots
- QRX Drive application publishing
- QRX-Net package distribution/discovery
- public/decentralized QRX App Directory
- update channels / reputation metadata
- Local Compute and QRX Compute app capability grants

## Verification

- Full CTest: 122/122 PASS (`audit/CTEST_APP_FOUNDATION_0.0.9_122_OF_122.log`)
- Changed C upscaler paths under ASan+UBSan: 2/2 PASS (`audit/SANITIZER_APP_FOUNDATION.log`)
- Phase 189 static/package gate validates package layout, sandbox attributes, bridge permissions, Mini SDK surface and 0.0.10 boundary.
- Main wallet inline JavaScript, App Host JavaScript, Mini SDK and demo JavaScript pass Node syntax checks.

Native Tauri/Rust compilation was not executable in the current container because `cargo`/`rustc` are not installed. The Rust implementation is included and must be compiled on the normal native wallet CI runners before a binary release.
