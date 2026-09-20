#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
cf=(r/'qrx-core/src/core_frontend.c').read_text()
pc=(r/'qrx-core/src/network/qrx_p2p_config.c').read_text()
ph=(r/'qrx-core/src/network/qrx_p2p_config.h').read_text()
qd=(r/'qrx-core/src/qrxd.c').read_text()
peers=['node01.qrx.mey-solution.de','89.143.197.28','174.165.210.227']
for x in peers:
    assert x+':26660' in cf, x
    assert '"'+x+'"' in pc, x
assert '#define QRX_SEEDNODE_COUNT 6' in ph
assert 'json_peer_lines_array(arr1,sizeof(arr1),out1)' in qd
assert 'strcmp(line,"[peers]")' in qd and 'strcmp(line,"[known]")' in qd
assert 'strcmp(line,"no peers")' in qd
print('QRX 0.0.9.84 mainnet peer bootstrap audit: PASS')
