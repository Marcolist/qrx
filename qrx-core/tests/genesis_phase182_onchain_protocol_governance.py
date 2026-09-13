#!/usr/bin/env python3
import os, pathlib, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit('usage: genesis_phase182_onchain_protocol_governance.py <qrx>')
qrx = pathlib.Path(sys.argv[1]).resolve()
if not qrx.exists():
    raise SystemExit('qrx executable missing')

env = os.environ.copy()
env['QRX_PASSPHRASE'] = 'Phase182-Wallet-Passphrase!'
env['QRX_GOV_PASSPHRASE'] = 'Phase182-Governance-Passphrase!'

def run(*args, ok=True):
    p = subprocess.run([str(qrx), *map(str,args)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    if ok and p.returncode != 0:
        raise AssertionError(f"command failed: {args}\nstdout={p.stdout}\nstderr={p.stderr}")
    if not ok and p.returncode == 0:
        raise AssertionError(f"command unexpectedly succeeded: {args}\nstdout={p.stdout}")
    return p

def ed_hex(wallet):
    cmd = f"openssl pkey -pubin -in '{wallet}/ed25519_pub.pem' -outform DER 2>/dev/null | tail -c 32 | od -An -tx1 | tr -d ' \\n'"
    return subprocess.check_output(['bash','-lc',cmd], text=True).strip()

def ml_b64(wallet):
    import base64
    return base64.b64encode((wallet/'mldsa65_pub.pem').read_bytes()).decode()

def make_tx(root, chain, wallet, addr, ed, ml, payload, tag, nonce=None):
    raw=root/f'{tag}.raw'; signed=root/f'{tag}.signed'
    args=['create-velocity-raw-tx',chain,addr,addr,'0',ed,ml,'GOVERNANCE_PROTOCOL','6','5000',payload]
    if nonce is not None:
        args += ['1000', str(nonce)]
    p=run(*args)
    raw.write_text(p.stdout)
    run('signrawtransactionwithwallet',wallet,chain,raw,signed)
    return signed

with tempfile.TemporaryDirectory(prefix='qrx-phase182-') as td:
    root=pathlib.Path(td); chain=root/'chain'; wallet=root/'wallet'
    run('seed-new',wallet)
    addr=(wallet/'address.txt').read_text().strip(); ed=ed_hex(wallet); ml=ml_b64(wallet)
    run('init-chain',chain,'20','5000','2100000000000000','25000000','1000000000000','qrx-regtest','9','QRXP182','Phase182')
    run('faucet',chain,addr,'1000000')
    keys=[]
    for i in range(1,6):
        d=root/f'gov{i}'; run('governance-keygen',d,f'DEV_GOV_{i}'); keys.append(d)
    run('governance-genesis-init',chain,'3',*(k/'governance.pub' for k in keys))

    # Convert this isolated fixture to Mainnet semantics only after test funding
    # and roots are installed. Genesis hash remains immutable and all v2
    # signatures bind to it.
    meta=chain/'chain.meta'; meta.write_text(meta.read_text().replace('network_id=qrx-regtest','network_id=qrx-mainnet-phase182'))

    # Whitehat regression: a local schedule file must have zero Mainnet authority.
    govdir=chain/'governance'; govdir.mkdir(exist_ok=True)
    (govdir/'protocol_upgrades.db').write_text('50|9|3|4|DRIVE_V1|local-attack\n')
    pre=run('resource-info',chain,'50').stdout
    assert 'activation_scheduled=false' in pre and 'storage_protocol_active=false' in pre

    proposal=root/'drive.proposal'
    run('governance-protocol-propose-v2',chain,proposal,'9','100','3','4','DRIVE_V1')
    sigs=[]
    for i in range(3):
        s=root/f'sig{i+1}'; run('governance-sign-v2',chain,proposal,keys[i],s); sigs.append(s)

    # Duplicate signers do not count as 3-of-5.
    dup_payload=run('governance-payload-v2',proposal,sigs[0],sigs[1],sigs[0]).stdout.split('payload=',1)[1].strip()
    bad=make_tx(root,chain,wallet,addr,ed,ml,dup_payload,'duplicate')
    run('verify',chain,bad,ok=False)

    payload=run('governance-payload-v2',proposal,*sigs).stdout.split('payload=',1)[1].strip()
    signed=make_tx(root,chain,wallet,addr,ed,ml,payload,'valid')
    run('verify',chain,signed)
    out=run('applytx',chain,signed).stdout
    assert 'APPLIED' in out

    before=run('resource-info',chain,'99').stdout
    at=run('resource-info',chain,'100').stdout
    assert 'activation_scheduled=true' in before
    assert 'activation_height=100' in before
    assert 'storage_protocol_active=false' in before
    assert 'storage_protocol_active=true' in at

    # Same signed governance transaction and same proposal cannot be replayed.
    run('applytx',chain,signed,ok=False)
    replay=make_tx(root,chain,wallet,addr,ed,ml,payload,'proposal-replay',nonce=2)
    run('verify',chain,replay,ok=False)

print('phase182: PASS - 3-of-5 v2 GOVERNANCE_PROTOCOL is consensus state; duplicate signers/replays fail and local Mainnet schedules have no authority')
