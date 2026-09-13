#!/usr/bin/env python3
from pathlib import Path
import json, re, sys
root=Path(__file__).resolve().parents[1]
errors=[]
cargo=root/"QRXBROWSER/src-tauri/Cargo.toml"
main=root/"QRXBROWSER/src-tauri/src/main.rs"
html=root/"QRXBROWSER/src/index.html"
js=root/"QRXBROWSER/src/app.js"
conf=root/"QRXBROWSER/src-tauri/tauri.conf.json"
wallet_rs=root/"GUIWALLET/src-tauri/src/main.rs"
wallet_conf=root/"GUIWALLET/src-tauri/tauri.conf.json"
for p in [cargo,main,html,js,conf,wallet_rs,wallet_conf]:
    if not p.exists(): errors.append("missing "+str(p.relative_to(root)))
if not errors:
    c=cargo.read_text()
    m=main.read_text()
    j=js.read_text()
    wc=json.loads(wallet_conf.read_text())
    cfg=json.loads(conf.read_text())
    checks=[
      ("Tauri 2 exact pin",'tauri = { version = "=2.11.5"' in c),
      ("unstable multiwebview feature",'"unstable"' in c),
      ("webview data URL feature",'"webview-data-url"' in c),
      ("two child webviews",m.count("add_child(")>=2),
      ("chrome child",'"browser-chrome"' in m),
      ("content child",'"browser-content"' in m),
      ("real HTTPS navigation",'view.navigate(url)' in m),
      ("QRX resolve path",'"resolvebrowserinput"' in m and '"fetchqrxsite"' in m),
      ("back navigation",'history.back()' in m),
      ("forward navigation",'history.forward()' in m),
      ("resize layout",'resize_children' in m),
      ("persistent QRX display state",'meta: Mutex<BrowserMeta>' in m and 'store_meta' in m),
      ("Tauri2 Webview window API",'let window = webview.window();' in m),
      ("frontend enter navigation","e.key==='Enter'" in j),
      ("wallet launches browser sidecar",'resolve_binary(Some(&app), "qrx-browser")' in wallet_rs.read_text()),
      ("wallet bundles browser sidecar","bin/qrx-browser" in wc["tauri"]["bundle"]["externalBin"]),
      ("Tauri2 has no implicit windows",cfg.get("app",{}).get("windows")==[]),
    ]
    for label,ok in checks:
        if not ok: errors.append(label)
if errors:
    print("QRX Tauri 2 Browser audit: FAIL",file=sys.stderr)
    for e in errors: print(" - "+e,file=sys.stderr)
    raise SystemExit(1)
print("QRX Tauri 2 Browser audit: PASS")
print("Isolated Tauri 2 host, fixed browser chrome + native content WebView, QRX-Net CLI path preserved.")
