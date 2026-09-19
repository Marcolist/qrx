#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
grep -q 'fn upscaler_start_local' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'fn upscaler_job_status' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'fn upscaler_cancel_job' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'fn upscaler_video_pipeline_plan' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
grep -q 'Possibly stalled' "$ROOT/GUIWALLET/src/upscaler/index.html"
grep -q 'remux-original-audio' "$ROOT/GUIWALLET/src-tauri/src/main.rs"
echo 'QRX 0.0.9.67 async upscaler/video pipeline audit: PASS'
