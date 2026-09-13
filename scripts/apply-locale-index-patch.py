#!/usr/bin/env python3
import json, pathlib, sys
if len(sys.argv)!=3:
    raise SystemExit('usage: apply-locale-index-patch.py <lang> <patch.tsv>')
root=pathlib.Path(__file__).resolve().parents[1]
loc=root/'GUIWALLET/src/locales'
lang=sys.argv[1]; patch=pathlib.Path(sys.argv[2])
en=json.loads((loc/'en.json').read_text(encoding='utf-8'))
d=json.loads((loc/f'{lang}.json').read_text(encoding='utf-8'))
items=list(en.items())
seen=set()
for no,line in enumerate(patch.read_text(encoding='utf-8').splitlines(),1):
    if not line.strip() or line.lstrip().startswith('#'): continue
    try: sidx, val=line.split('\t',1)
    except ValueError: raise SystemExit(f'{patch}:{no}: expected index<TAB>translation')
    idx=int(sidx)
    if not (0<=idx<len(items)): raise SystemExit(f'{patch}:{no}: bad index {idx}')
    if idx in seen: raise SystemExit(f'{patch}:{no}: duplicate index {idx}')
    seen.add(idx)
    key,src=items[idx]
    val=val.replace('\\n','\n')
    d[key]=val
(loc/f'{lang}.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'{lang}: applied {len(seen)} indexed translations')
