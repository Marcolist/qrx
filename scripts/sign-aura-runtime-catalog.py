#!/usr/bin/env python3
import argparse, hashlib, os, struct, subprocess, tempfile
from pathlib import Path

ADAPTER={'LLAMA_CPP_CPU':1,'LLAMA_CPP_CUDA':2,'MLX_METAL':3,'EXTERNAL':4,'LLAMA_CPP_METAL':5}
PLATFORM={'any':0,'linux':1,'macos':2,'windows':3}
ARCH={'any':0,'x86_64':1,'aarch64':2,'arm':3}
BACKEND={'cpu':1,'mlx_metal':2,'cuda':3,'other':4}
FEATURE={'FP16':1<<0,'FP32':1<<1,'INT8':1<<2,'TENSOR_CORES':1<<3,'UNIFIED_MEM':1<<4,'MLX':1<<5,'METAL':1<<6,'CUDA':1<<7}
DOMAIN=b'QRX/AURA/RUNTIME-PACKAGE/V1'

def envfile(path):
    d={}
    for line in Path(path).read_text().splitlines():
        line=line.strip()
        if not line or line.startswith('#'): continue
        k,v=line.split('=',1); d[k]=v
    return d

def sha3file(path):
    h=hashlib.sha3_256()
    with open(path,'rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()

def u32(v): return struct.pack('>I',int(v))
def u64(v): return struct.pack('>Q',int(v))
def s(v):
    b=v.encode(); return u32(len(b))+b

def record(meta, meta_path):
    base=Path(meta_path).parent
    artifact=base/meta['artifact']
    got=sha3file(artifact)
    if got != meta['content_root']: raise SystemExit(f'content root mismatch: {artifact}')
    feat=0
    for name in filter(None,meta['required_features'].split(',')): feat |= FEATURE[name]
    a=ADAPTER[meta['adapter']]; plat=PLATFORM[meta['platform']]; arch=ARCH[meta['arch']]; backend=BACKEND[meta['backend']]
    vals=[1,meta['publisher_id'],meta['package_id'],meta['package_version'],int(meta['sequence']),int(meta['valid_from_height']),int(meta['valid_until_height']),a,plat,arch,backend,feat,int(meta['min_memory_mib'])*1024*1024,int(meta['cuda_major']),int(meta['cuda_minor']),got,meta['download_uri'],meta['install_filename']]
    payload=u32(vals[0])+s(vals[1])+s(vals[2])+s(vals[3])+u64(vals[4])+u64(vals[5])+u64(vals[6])+u32(vals[7])+u32(vals[8])+u32(vals[9])+u32(vals[10])+u32(vals[11])+u64(vals[12])+u32(vals[13])+u32(vals[14])+s(vals[15])+s(vals[16])+s(vals[17])
    digest=hashlib.sha3_256(DOMAIN+payload).digest()
    return vals,digest

def sign(key,digest):
    with tempfile.TemporaryDirectory() as td:
        ip=Path(td)/'digest.bin'; op=Path(td)/'sig.bin'; ip.write_bytes(digest)
        subprocess.run(['openssl','pkeyutl','-sign','-rawin','-inkey',str(key),'-in',str(ip),'-out',str(op)],check=True)
        return op.read_bytes()

def main():
    ap=argparse.ArgumentParser(description='Build a signed QRX AURA runtime catalog from native runtime metadata.')
    ap.add_argument('--key',required=True,help='Ed25519 private key PEM (release secret; never commit)')
    ap.add_argument('--output',required=True)
    ap.add_argument('--public-key-output')
    ap.add_argument('metadata',nargs='+')
    args=ap.parse_args()
    rows=[]
    for mp in args.metadata:
        m=envfile(mp); vals,digest=record(m,mp); sig=sign(args.key,digest)
        if len(sig)>1024: raise SystemExit('signature unexpectedly large')
        rows.append((vals[2], '|'.join(str(x) for x in vals)+'|'+sig.hex()))
    rows.sort()
    out=Path(args.output); out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text('QRXRUNTIME1\n'+'\n'.join(r for _,r in rows)+'\n')
    if args.public_key_output:
        subprocess.run(['openssl','pkey','-in',args.key,'-pubout','-out',args.public_key_output],check=True)
    print(f'wrote {len(rows)} signed runtime packages to {out}')
if __name__=='__main__': main()
