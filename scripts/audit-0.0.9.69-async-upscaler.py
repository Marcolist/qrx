#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
html=(root/"GUIWALLET/src/upscaler/index.html").read_text(errors="replace")
rust=(root/"GUIWALLET/src-tauri/src/main.rs").read_text(errors="replace")
errors=[]
cmds=["upscaler_start_local","upscaler_job_status","upscaler_cancel_job","upscaler_video_pipeline_plan"]
for c in cmds:
    if c not in html: errors.append(f"GUI missing async command: {c}")
    if rust.count(c)<2: errors.append(f"Rust command not defined/registered: {c}")
legacy="upscaler_"+"run_local"
if legacy in html or legacy in rust:
    errors.append("legacy synchronous upscaler command is present")
if errors:
    print("QRX 0.0.9.69 async upscaler audit: FAIL", file=sys.stderr)
    [print(" - "+e,file=sys.stderr) for e in errors]
    raise SystemExit(1)
print("QRX 0.0.9.69 async upscaler audit: PASS")
print("Validated start/status/cancel/video-plan bridge and legacy sync-command removal.")
