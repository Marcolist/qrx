from pathlib import Path
root=Path(__file__).resolve().parents[2]
q=(root/'qrx-core/src/qrxd.c').read_text()
c=(root/'qrx-core/src/qrx_cli.c').read_text()
o=(root/'scripts/qrx-mainnet-observatory.py').read_text()
p=(root/'scripts/qrx-peer-viewer.py').read_text()
u=(root/'scripts/qrx-emergency-upgrade-check.py').read_text()
assert 'getmainnethealth' in q and 'supply-invariant' in q and 'state-root' in q and 'protocol-info' in q
assert 'getmainnethealth' in c
assert 'fork/state-root disagreement' in o and 'supply invariant failed' in o
assert 'known_peers.txt' in p and 'DDoS' in p
assert 'agreement>2/3' in u and 'min-lead-blocks' in u
print('phase7217 observatory assertions PASS')
