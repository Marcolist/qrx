# QRX 0.0.9.67 — Async Upscaler Jobs & Video Pipeline Foundation

This release removes blocking local Upscaler execution from the Tauri IPC/UI path. Image and classical video work now start as background jobs with pollable state, cancellation, elapsed time, runtime activity age and a bounded live log. The GUI uses indeterminate progress while the upstream runtime does not expose trustworthy percentages rather than presenting fabricated progress.

The Video Pipeline foundation defines deterministic frame identity, resumable batches, result verification, ordered merge and original-audio remux. QRX Compute is an explicit opt-in execution target and remains activation-gated; 0.0.9.67 does not claim that remote provider dispatch is live.
