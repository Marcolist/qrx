#!/usr/bin/env bash
set -euo pipefail
src="${1:-$(dirname "$0")/demo-hello}"
out="${2:-$(dirname "$0")/dist/$(basename "$src").qrxapp}"
mkdir -p "$(dirname "$out")"
python3 - "$src" "$out" <<'PY'
import os,sys,zipfile,json
src,out=sys.argv[1:]
with open(os.path.join(src,'qrx-app.json'),'r',encoding='utf-8') as f:m=json.load(f)
assert m.get('format')==1 and m.get('id') and m.get('entry')
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
  for root,dirs,files in os.walk(src):
    dirs[:]=[d for d in dirs if not d.startswith('.')]
    for name in sorted(files):
      if name.startswith('.'):continue
      p=os.path.join(root,name);arc=os.path.relpath(p,src).replace(os.sep,'/')
      z.write(p,arc)
print(out)
PY
