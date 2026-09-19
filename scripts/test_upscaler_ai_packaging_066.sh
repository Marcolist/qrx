#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
for f in "$R/GUIWALLET/src-tauri/tauri.conf.json" "$R/GUIWALLET/src-tauri/tauri.macos.conf.json" "$R/GUIWALLET/src-tauri/tauri.linux.conf.json" "$R/GUIWALLET/src-tauri/tauri.windows.conf.json"; do
  python3 - "$f" <<'PY'
import json,sys
p=sys.argv[1]; d=json.load(open(p))
r=d['tauri']['bundle']['resources']
assert 'resources/upscaler/**/*' in r, f'upscaler resource glob missing from {p}'
PY
done
grep -q 'AI bundle inside GUI Wallet.app: PASS (staged/package hashes identical)' "$R/scripts/build-all-targets.sh"
grep -q 'find . -type f -print0' "$R/scripts/build-all-targets.sh"
echo 'QRX 0.0.9.66 Tauri AI resource packaging audit: PASS'
