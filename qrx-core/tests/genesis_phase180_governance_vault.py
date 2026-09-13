#!/usr/bin/env python3
import os, pathlib, shutil, subprocess, sys, tempfile

if len(sys.argv) != 2:
    raise SystemExit('usage: genesis_phase180_governance_vault.py <qrx>')
qrx = pathlib.Path(sys.argv[1]).resolve()
if not qrx.exists():
    raise SystemExit('qrx executable missing')

env = os.environ.copy()
env['QRX_GOV_PASSPHRASE'] = 'Phase180-Test-Governance-Passphrase!'

def run(*args, ok=True, env_override=None):
    e = env.copy()
    if env_override:
        e.update(env_override)
    p = subprocess.run([str(qrx), *map(str,args)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=e)
    if ok and p.returncode != 0:
        raise AssertionError(f"command failed: {args}\nstdout={p.stdout}\nstderr={p.stderr}")
    if not ok and p.returncode == 0:
        raise AssertionError(f"command unexpectedly succeeded: {args}\nstdout={p.stdout}")
    return p

with tempfile.TemporaryDirectory(prefix='qrx-phase180-') as td:
    root = pathlib.Path(td)
    keys=[]
    for i in range(1,6):
        d=root/f'GOV{i}'
        run('governance-keygen', d, f'DEV_GOV_{i}')
        keys.append(d)

    backup=root/'backup-vault'
    p=run('governance-vault-backup-create', backup, *keys)
    assert 'vault_type=OFFLINE_BACKUP' in p.stdout
    assert 'signing_disabled=true' in p.stdout
    for i in range(1,6):
        slot=backup/'keys'/f'DEV_GOV_{i}'
        assert (slot/'governance.key.backup').exists()
        assert not (slot/'governance.key').exists()

    online=root/'operational-vault'
    run('governance-vault-init', online)
    run('governance-vault-restore-online', backup, 'DEV_GOV_1', online)
    run('governance-vault-restore-online', backup, 'DEV_GOV_2', online)
    for i in range(3,6):
        run('governance-vault-add-offline', online, keys[i-1]/'governance.pub')

    # Imported descriptors are hostile input: path-like key IDs and forged
    # fingerprints must fail before any slot path is created.
    hostile_vault=root/'hostile-vault'
    run('governance-vault-init', hostile_vault)
    good=(keys[2]/'governance.pub').read_text()
    bad_id=root/'bad-id.pub'
    bad_id.write_text(good.replace('key_id=DEV_GOV_3','key_id=../../escape'))
    run('governance-vault-add-offline', hostile_vault, bad_id, ok=False)
    assert not (root/'escape').exists()
    bad_fp=root/'bad-fp.pub'
    lines=[]
    for line in good.splitlines():
        lines.append('fingerprint=' + '0'*32 if line.startswith('fingerprint=') else line)
    bad_fp.write_text('\n'.join(lines)+'\n')
    run('governance-vault-add-offline', hostile_vault, bad_fp, ok=False)

    st=run('governance-vault-status', online).stdout
    assert 'entries=5' in st
    assert 'online_signers=2' in st
    assert st.count('role=ONLINE') == 2
    assert st.count('role=OFFLINE') == 3

    # A third online signer must fail closed.
    p=run('governance-vault-restore-online', backup, 'DEV_GOV_3', online, ok=False)
    assert 'already has two online signers' in p.stderr

    # Backup vault cannot sign through the vault API, and its backup filename
    # cannot be consumed accidentally by the legacy governance-sign command.
    proposal=root/'proposal.qrx'
    run('governance-protocol-propose', proposal, '9', '100', '1', '0', 'DRIVE_V1')
    p=run('governance-vault-sign', backup, 'DEV_GOV_1', proposal, root/'bad.sig', ok=False)
    assert 'signing is disabled' in p.stderr
    p=run('governance-sign', backup/'keys'/'DEV_GOV_1', proposal, root/'bad2.sig', ok=False)
    assert 'cannot unlock governance private key' in p.stderr

    # Two online signatures + one separately produced offline signature satisfy
    # the unchanged 3-of-5 consensus threshold.
    sig1=root/'sig1.qrx'; sig2=root/'sig2.qrx'; sig3=root/'sig3.qrx'
    run('governance-vault-sign', online, 'DEV_GOV_1', proposal, sig1)
    run('governance-vault-sign', online, 'DEV_GOV_2', proposal, sig2)
    run('governance-sign', keys[2], proposal, sig3)  # simulates the offline signer machine

    chain=root/'chain'
    run('init-chain', chain, '20','5000','2100000000000000','25000000','100000000000','qrx-regtest','9','QRXTEST','Regtest')
    run('governance-genesis-init', chain, '3', *(k/'governance.pub' for k in keys))
    applied=run('governance-apply', chain, proposal, sig1, sig2, sig3).stdout
    assert 'valid_unique_signatures=3' in applied
    assert 'threshold=3' in applied

print('phase180: PASS - offline five-key backup + max-two operational signers + 3-of-5 offline signature workflow')
