# QRX 0.0.9.45 — Signed Runtime Packages, Hardware Auto-Detection & One-Click AURA Activation

Implemented:
- `qrx_aura_runtime_packages.{h,c}` integrated into qrxcore.
- signed/versioned runtime package announcements with platform, architecture, backend, hardware requirements, validity heights and anti-rollback sequence.
- native CPU/CUDA/Apple Metal inventory reuse from the existing 0.0.9 runtime discovery path.
- verified SHA3 package install with atomic replacement.
- one-click orchestration: discover → select → fetch callback → verify/install → activate → persist host config.
- Apple Metal → MLX trust transition only after a verified MLX package is selected.
- phase170 test includes tamper rejection and catalog anti-rollback.

Validation: phase166–170 PASS; later full suite PASS.
