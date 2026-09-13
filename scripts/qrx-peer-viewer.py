#!/usr/bin/env python3
import argparse,ipaddress,socket,os

def classify(h):
    try:
        ip=ipaddress.ip_address(h.strip('[]'))
        return 'IP', 'private' if ip.is_private else ('loopback' if ip.is_loopback else 'public')
    except ValueError: return 'DNS','hostname'

def resolve(h):
    try: return sorted({x[4][0] for x in socket.getaddrinfo(h,None,type=socket.SOCK_STREAM)})
    except Exception: return []

def main():
    ap=argparse.ArgumentParser(description='QRX peer viewer: configured/known peer domains and IPs')
    ap.add_argument('node_dir'); ap.add_argument('--resolve',action='store_true',help='resolve DNS names locally'); a=ap.parse_args()
    found=[]
    for src in ('peers.txt','known_peers.txt','seeds.txt'):
        p=os.path.join(a.node_dir,src)
        if not os.path.exists(p): continue
        for line in open(p,errors='ignore'):
            s=line.strip()
            if not s or s.startswith('#') or s.startswith('['): continue
            host=s; port=''
            if s.startswith('[') and ']:' in s: host,port=s[1:].split(']:',1)
            elif ':' in s: host,port=s.rsplit(':',1)
            typ,scope=classify(host); ips=resolve(host) if a.resolve and typ=='DNS' else ([host] if typ=='IP' else [])
            found.append((src,host,port,typ,scope,','.join(ips) or '-'))
    print(f"{'SOURCE':16} {'HOST/DOMAIN':42} {'PORT':7} {'TYPE':5} {'SCOPE':10} RESOLVED_IPS")
    for r in sorted(set(found)): print(f"{r[0]:16} {r[1]:42} {r[2]:7} {r[3]:5} {r[4]:10} {r[5]}")
    print(f"\nunique_peers={len(set((r[1],r[2]) for r in found))}")
    print('privacy_note=This viewer shows network endpoints already known to your node. Do not publish a full peer list by default; validator IP disclosure can increase DDoS targeting risk.')
if __name__=='__main__': main()
