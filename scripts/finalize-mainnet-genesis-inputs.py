#!/usr/bin/env python3
"""QRX 0.0.7.7 Phase 7.2 final Mainnet Genesis input patcher.

Inputs:
  --validators FILE       exactly 50 qrx1 + 120-hex addresses
  --governance FILE...    exactly 5 governance.pub descriptors (or raw 64-hex pubkeys)

Only public material is written to source. Private keys/seeds are never read.
"""
from pathlib import Path
import argparse, hashlib, re, sys

ROOT = Path(__file__).resolve().parents[1]
VALSRC = ROOT/'qrx-core/src/genesis/qrx_bootstrap_validators.c'
GOVSRC = ROOT/'qrx-core/src/genesis/qrx_genesis_governance.c'
ADDR_RE = re.compile(r'^qrx1[0-9a-fA-F]{120}$')
HEX64 = re.compile(r'^[0-9a-fA-F]{64}$')

def clean_lines(path):
    return [x.strip() for x in Path(path).read_text().splitlines() if x.strip() and not x.lstrip().startswith('#')]

def governance_pub(path_or_hex, expected_id):
    p=Path(path_or_hex)
    if p.exists():
        vals={}
        for line in p.read_text().splitlines():
            if '=' in line:
                k,v=line.split('=',1); vals[k.strip()]=v.strip()
        pub=vals.get('public_key_hex','')
        kid=vals.get('key_id',expected_id)
        if kid != expected_id:
            raise ValueError(f'{p}: expected key_id={expected_id}, got {kid}')
        fp=vals.get('fingerprint')
        if fp:
            calc=hashlib.sha3_512(f'QUB-GOVERNANCE-ROOT-v1|{pub}'.encode()).hexdigest()[:32]
            if calc.lower()!=fp.lower(): raise ValueError(f'{p}: fingerprint mismatch')
    else:
        pub=path_or_hex.strip()
    if not HEX64.fullmatch(pub): raise ValueError(f'{expected_id}: public key must be exactly 64 hex chars')
    return pub.lower()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--validators',required=True)
    ap.add_argument('--governance',nargs=5,required=True,metavar=('DEV_GOV_1','DEV_GOV_2','DEV_GOV_3','DEV_GOV_4','DEV_GOV_5'))
    ap.add_argument('--check-only',action='store_true')
    args=ap.parse_args()
    vals=clean_lines(args.validators)
    if len(vals)!=50: raise SystemExit(f'ERROR: expected exactly 50 validator addresses, got {len(vals)}')
    for i,a in enumerate(vals,1):
        if not ADDR_RE.fullmatch(a): raise SystemExit(f'ERROR: validator {i} is not a canonical qrx1 + 120-hex address: {a}')
    if len(set(x.lower() for x in vals))!=50: raise SystemExit('ERROR: duplicate validator address')
    pubs=[]
    try:
        for i,g in enumerate(args.governance,1): pubs.append(governance_pub(g,f'DEV_GOV_{i}'))
    except ValueError as e: raise SystemExit(f'ERROR: {e}')
    if len(set(pubs))!=5: raise SystemExit('ERROR: governance public keys must be unique')
    digest=hashlib.sha3_512(('QRX-MAINNET-GENESIS-INPUTS-v1\n'+'\n'.join(vals+pubs)).encode()).hexdigest()
    print(f'validators=50\ngovernance_roots=5\ngovernance_threshold=3\ninput_commitment_sha3_512={digest}')
    if args.check_only:
        print('status=VALIDATED_NOT_WRITTEN'); return
    vs=VALSRC.read_text()
    # Replace the first quoted address in each of the 50 initializer rows, preserving policy constants.
    rows=re.findall(r'\{\"[^\"]+\", QRX_BOOTSTRAP_VALIDATOR_ATOMS, QRX_BOOTSTRAP_LOCK_UNTIL_HEIGHT, 1, 0\}',vs)
    if len(rows)!=50: raise SystemExit(f'ERROR: expected 50 bootstrap initializer rows in {VALSRC}, found {len(rows)}')
    for old,new in zip(rows,vals):
        vs=vs.replace(old,f'{{"{new}", QRX_BOOTSTRAP_VALIDATOR_ATOMS, QRX_BOOTSTRAP_LOCK_UNTIL_HEIGHT, 1, 0}}',1)
    VALSRC.write_text(vs)
    gs=GOVSRC.read_text()
    for i,pub in enumerate(pubs,1):
        pattern=re.compile(r'(\{"DEV_GOV_%d", \")[^\"]+(\"\})'%i)
        gs,n=pattern.subn(r'\g<1>'+pub+r'\g<2>',gs,count=1)
        if n!=1: raise SystemExit(f'ERROR: could not update DEV_GOV_{i} in {GOVSRC}')
    GOVSRC.write_text(gs)
    print('status=WRITTEN')
    print(f'validator_source={VALSRC}')
    print(f'governance_source={GOVSRC}')
    print('next=compile QRX; Mainnet interlock will unlock only if both inputs are valid')
if __name__=='__main__': main()
