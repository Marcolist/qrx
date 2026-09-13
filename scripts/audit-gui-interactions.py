#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1];src=root/"GUIWALLET"/"src";errors=[]
required={
"index.html":["installInlineActionBridge(document)","auraSendBtn","auraWindowBtn","auraFullBtn"],
"browser/index.html":["bindBrowserControls()","open_browser_www_window"],
"upscaler/index.html":["chooseMediaBtn","processBtn","upscaler_run_local"],
"generals/index.html":["QRX_GENERAL_ACTIONS","addEventListener('click'"],
"aura/index.html":["id=\"ask\"","addEventListener('keydown'"]}
for rel,tokens in required.items():
 p=src/rel
 if not p.exists(): errors.append(f"missing GUI surface: {rel}"); continue
 text=p.read_text(errors="replace")
 for token in tokens:
  if token not in text: errors.append(f"{rel}: missing interaction token {token!r}")
rs=(root/"GUIWALLET"/"src-tauri"/"src"/"main.rs").read_text(errors="replace")
for cmd in ["open_browser_www_window","upscaler_run_local","open_aura_window","open_generals_window","open_qrx_browser_window","open_qrx_upscaler_window"]:
 if rs.count(cmd)<2: errors.append(f"Rust command not both defined/registered: {cmd}")
if errors:
 print("QRX GUI interaction audit: FAIL",file=sys.stderr)
 for e in errors: print(" - "+e,file=sys.stderr)
 raise SystemExit(1)
print("QRX GUI interaction audit: PASS")
print("Validated wallet/AURA, Browser, Upscaler and Generals interaction bridges.")
