# QRX 0.0.9.67 — Async Upscaler Jobs & Video Pipeline Foundation

- Upscaler execution moved off the Tauri command/UI thread.
- Pollable job state: phase, elapsed time, last runtime activity, bounded live log, completion/error and cancellation.
- Indeterminate progress is used when upstream Real-ESRGAN exposes no trustworthy percentage; QRX does not fake progress.
- Stall UX flags 60s without child-process output while keeping the UI responsive.
- Video pipeline planner defines: probe → extract frames → hash manifest → batches → upscale → verify → ordered merge → remux original audio.
- Batch journal/resume and deterministic frame identity are part of the contract.
- QRX Compute dispatch is explicit opt-in and activation-gated (`COMPUTE_POUC_V1`); this release provides the orchestration contract, not a claim of active distributed execution.
