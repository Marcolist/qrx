#!/usr/bin/env python3
from pathlib import Path
import json, sys, re
try: from bs4 import BeautifulSoup
except Exception: print('BeautifulSoup required for audit'); sys.exit(2)
root=Path(__file__).resolve().parents[1]/'GUIWALLET/src'
loc=root/'locales'; en=json.loads((loc/'en.json').read_text()); keys=set(en)
manifest=json.loads((loc/'manifest.json').read_text())
err=[]
for lang in manifest['languages']:
 d=json.loads((loc/f'{lang}.json').read_text())
 if set(d)!=keys: err.append(f'{lang}: key mismatch missing={len(keys-set(d))} extra={len(set(d)-keys)}')
for f in [root/'index.html',root/'generals/index.html',root/'app-host/index.html']:
 s=BeautifulSoup(f.read_text(),'html.parser')
 for e in s.find_all(True):
  for a in ('data-i18n','data-i18n-title','data-i18n-placeholder','data-i18n-aria-label'):
   if e.has_attr(a) and e[a] not in keys: err.append(f'{f.name}: unknown {a}={e[a]}')
 raw=f.read_text()
 for k in re.findall(r"(?:tr|qrxFmt|ht)\(\s*['\"]([^'\"]+)['\"]",raw):
  if k not in keys and not k.startswith(('safety.','health.')): err.append(f'{f.name}: unknown dynamic i18n key={k}')
print(f'QRX i18n audit: {len(manifest["languages"])} locales, {len(keys)} keys')
if err:
 print('\n'.join(err)); sys.exit(1)
print('PASS: complete key parity across all locale catalogs')
