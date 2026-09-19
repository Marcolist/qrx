#!/usr/bin/env python3
from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
b=(r/'qrx-core/src/genesis/qrx_bootstrap_validators.c').read_text()
a=re.findall(r'\{"(qrx1[0-9a-f]+)", QRX_BOOTSTRAP_VALIDATOR_ATOMS',b)
assert len(a)==60 and len(set(a))==60
assert all(len(x)==124 for x in a)
h=(r/'qrx-core/src/genesis/qrx_bootstrap_validators.h').read_text()
assert '#define QRX_BOOTSTRAP_VALIDATOR_COUNT 60' in h
assert '#define QRX_BOOTSTRAP_VALIDATOR_QUB 1000ULL' in h
g=(r/'qrx-core/src/genesis/qrx_genesis_governance.c').read_text()
assert 'REPLACE_WITH_DEV_GOV_' not in g
assert len(re.findall(r'\{"DEV_GOV_[1-5]", "[0-9a-f]{64}"\}',g))==5
gh=(r/'qrx-core/src/genesis/qrx_genesis_governance.h').read_text(); assert 'QRX_GENESIS_GOVERNANCE_THRESHOLD 3' in gh
d=(r/'qrx-core/src/qrxd.c').read_text(); assert 'const char *network="mainnet"' in d
c=(r/'qrx-core/src/qrx_cli.c').read_text(); assert 'snprintf(buf, buf_sz, "mainnet")' in c
ui=(r/'GUIWALLET/src/index.html').read_text(); assert 'const appState = { network:"mainnet"' in ui and 'selected="" value="mainnet"' in ui
assert 'qrx1bootstrap' not in b
print('QRX 0.0.9.77 mainnet genesis finalization audit: PASS')
