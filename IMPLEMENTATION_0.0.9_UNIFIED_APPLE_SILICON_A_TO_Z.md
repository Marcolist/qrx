# QRX 0.0.9 - Unified Apple Silicon Profile + A-Z Handbook Refresh

## Changes

- Collapsed M1/M2/M3/M4 Upscaler compatibility enums into one stable `apple-silicon` profile.
- Preserved detected Apple M generation and variant (`base`, `Pro`, `Max`, `Ultra`) as metadata for Auto-tuning and UI display.
- Kept Raspberry Pi 5 and ODROID-N2/N2+ as distinct profiles because their Vulkan driver/memory characteristics differ materially.
- Extended `qrx-upscaler capabilities` JSON with `apple_silicon_generation` and `apple_silicon_variant`.
- Updated Phase 188 regression coverage for the unified profile.
- Updated the Phase 174 documentation gate to accept the current 122/122 baseline.
- Expanded the canonical A-Z Handbook with the complete 0.0.9 QRX App Foundation, `.qrxapp` v1, App Registry, sandbox/App Host, permissions, Mini JS SDK, `Hello QRX`, Developer Mode, 0.0.10 ecosystem boundary, and detailed QRX Upscaler capability/workflow status.
- Corrected the handbook to distinguish implemented local/classical Upscaler functionality from not-yet-bundled AI model/runtime/media components.
