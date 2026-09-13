#!/usr/bin/env python3
import csv,json,pathlib,sys
if len(sys.argv)!=2: raise SystemExit('usage: apply-i18n-matrix.py <matrix.tsv>')
root=pathlib.Path(__file__).resolve().parents[1]
loc=root/'GUIWALLET/src/locales'
en=json.loads((loc/'en.json').read_text(encoding='utf-8'))
items=list(en.items())
with open(sys.argv[1],encoding='utf-8',newline='') as f:
    r=csv.DictReader(f,delimiter='\t')
    langs=[x for x in r.fieldnames if x!='idx']
    data={l:json.loads((loc/f'{l}.json').read_text(encoding='utf-8')) for l in langs}
    counts={l:0 for l in langs}
    seen=set()
    for no,row in enumerate(r,2):
        idx=int(row['idx'])
        if idx in seen: raise SystemExit(f'duplicate idx {idx} line {no}')
        seen.add(idx)
        if not (0<=idx<len(items)): raise SystemExit(f'bad idx {idx}')
        key,_=items[idx]
        for l in langs:
            val=row[l].replace('\\n','\n')
            if not val: raise SystemExit(f'empty {l} idx {idx}')
            data[l][key]=val; counts[l]+=1
for l,d in data.items():
    (loc/f'{l}.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(l,counts[l])
