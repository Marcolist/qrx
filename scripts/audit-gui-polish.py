#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
wallet=(root/"GUIWALLET/src/index.html").read_text(errors="replace")
generals=(root/"GUIWALLET/src/generals/index.html").read_text(errors="replace")
aura=(root/"GUIWALLET/src/aura/index.html").read_text(errors="replace")
browser=(root/"GUIWALLET/src/browser/index.html").read_text(errors="replace")
errors=[]
if generals.startswith("html"): errors.append("Generals starts with stray literal html")
for token in ["qrx_intro_accepted_v3","© 2026 QRX Chain Team","qrxWebsiteBtn","qrxDiscordBtn","qrxBitcointalkBtn","visual.length","auraLiveContext"]:
    if token not in wallet: errors.append("wallet missing "+token)
if "➤" not in aura: errors.append("standalone AURA paper-plane send missing")
if "compatBtn" not in browser or "embedded route" not in browser: errors.append("browser embedded/fallback route missing")
if errors:
    print("QRX GUI polish audit: FAIL",file=sys.stderr)
    for e in errors: print(" - "+e,file=sys.stderr)
    raise SystemExit(1)
print("QRX GUI polish audit: PASS")
