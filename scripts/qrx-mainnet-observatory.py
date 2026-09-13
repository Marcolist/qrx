#!/usr/bin/env python3
import argparse,json,subprocess,time,hashlib,os,sys

def run_cli(cli, datadir, network, method):
    p=subprocess.run([cli,'--network',network,'--datadir',datadir,method],capture_output=True,text=True,timeout=15)
    if p.returncode not in (0,2): return {'ok':False,'error':p.stderr.strip() or p.stdout.strip()}
    try: return json.loads(p.stdout)
    except Exception: return {'ok':False,'error':'invalid json','raw':p.stdout[-1000:]}

def unwrap(x):
    return x.get('result',{}) if isinstance(x,dict) else {}

def assess(rows):
    live=[r for r in rows if r.get('ok')]
    if not live: return 'CRITICAL',['no reachable nodes']
    reasons=[]; sev='GREEN'
    def bump(s):
        nonlocal sev
        order={'GREEN':0,'YELLOW':1,'RED':2,'CRITICAL':3}
        if order[s]>order[sev]: sev=s
    groups={}
    for r in live:
        k=(r.get('height'),r.get('bestblockhash'),json.dumps(r.get('state',{}),sort_keys=True))
        groups.setdefault(k,[]).append(r['name'])
        if r.get('blocks_behind',0)>2: bump('YELLOW'); reasons.append(f"{r['name']} behind={r.get('blocks_behind')}")
        if r.get('connections',0)<1: bump('YELLOW'); reasons.append(f"{r['name']} has no peers")
        si=r.get('supply_invariant',{})
        text=json.dumps(si).upper()
        if 'FAIL' in text or 'FALSE' in text: bump('CRITICAL'); reasons.append(f"{r['name']} supply invariant failed")
        proto=r.get('protocol',{})
        if str(proto.get('update_required','false')).lower()=='true': bump('RED'); reasons.append(f"{r['name']} update required")
    by_height={}
    for r in live: by_height.setdefault(r.get('height'),set()).add((r.get('bestblockhash'),json.dumps(r.get('state',{}),sort_keys=True)))
    for h,v in by_height.items():
        if len(v)>1: bump('CRITICAL'); reasons.append(f'fork/state-root disagreement at height {h}')
    hs=[r.get('height',0) for r in live]
    if max(hs)-min(hs)>2: bump('YELLOW'); reasons.append('node height spread >2')
    if len(live)<min(3,len(rows)): bump('RED'); reasons.append(f'only {len(live)}/{len(rows)} configured nodes reachable')
    return sev, sorted(set(reasons))

def main():
    ap=argparse.ArgumentParser(description='QRX Mainnet Observatory - multi-node convergence/fork/supply/protocol monitor')
    ap.add_argument('--config',required=True,help='JSON config; local datadirs only, tokens never leave host')
    ap.add_argument('--once',action='store_true'); ap.add_argument('--interval',type=int,default=15)
    ap.add_argument('--json-out',default='mainnet-health.json'); ap.add_argument('--events',default='mainnet-observatory-events.jsonl')
    a=ap.parse_args(); cfg=json.load(open(a.config)); cli=cfg.get('qrx_cli','qrx-cli'); net=cfg.get('network','mainnet')
    while True:
        rows=[]
        for n in cfg['nodes']:
            x=run_cli(cli,n['datadir'],net,'getmainnethealth'); r=unwrap(x) if x.get('ok') else {}
            r.update({'name':n['name'],'ok':bool(x.get('ok'))});
            if not x.get('ok'): r['error']=x.get('error','rpc failed')
            rows.append(r)
        sev,reasons=assess(rows); now=int(time.time())
        out={'timestamp':now,'network':net,'status':sev,'reasons':reasons,'nodes':rows,'node_count':len(rows),'reachable':sum(1 for r in rows if r['ok'])}
        tmp=a.json_out+'.tmp'; open(tmp,'w').write(json.dumps(out,indent=2,sort_keys=True)+'\n'); os.replace(tmp,a.json_out)
        with open(a.events,'a') as f: f.write(json.dumps({'timestamp':now,'status':sev,'reasons':reasons})+'\n')
        print(f"[{time.strftime('%F %T')}] QRX MAINNET {sev} reachable={out['reachable']}/{out['node_count']} " + ('; '.join(reasons) if reasons else 'converged'))
        if a.once: return 0 if sev in ('GREEN','YELLOW') else 2
        time.sleep(max(5,a.interval))
if __name__=='__main__': raise SystemExit(main())
