#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
wallet=(root/"GUIWALLET/src/index.html").read_text(encoding="utf-8", errors="replace")
generals=(root/"GUIWALLET/src/generals/index.html").read_text(encoding="utf-8", errors="replace")
aura=(root/"GUIWALLET/src/aura/index.html").read_text(encoding="utf-8", errors="replace")
browser=(root/"GUIWALLET/src/browser/index.html").read_text(encoding="utf-8", errors="replace")
errors=[]
if generals.startswith("html"): errors.append("Generals starts with stray literal html")
for token in ["qrx_intro_accepted_v3","© 2026 QRX Chain Team","qrxWebsiteBtn","qrxDiscordBtn","qrxBitcointalkBtn","visual.length","auraLiveContext"]:
    if token not in wallet: errors.append("wallet missing "+token)
# Do not audit a Unicode glyph: Windows locale/code-page differences can alter the
# decoded paper-plane character while the actual control remains valid. Audit the
# stable interaction contract instead: send button + ask handler + click wiring.
for token in ['id="ask"', 'async function ask()', "$('ask').onclick=ask"]:
    if token not in aura: errors.append("standalone AURA send interaction missing: "+token)
if "compatBtn" not in browser or "embedded route" not in browser: errors.append("browser embedded/fallback route missing")
if errors:
    print("QRX GUI polish audit: FAIL",file=sys.stderr)
    for e in errors: print(" - "+e,file=sys.stderr)
    raise SystemExit(1)
print("QRX GUI polish audit: PASS")
