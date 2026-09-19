#!/usr/bin/env python3
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'GUIWALLET/src/index.html'
s=p.read_text()
need=['function parsePeerOverview(v)','isPeerSectionMarker','isWildcardListenerHost','setText("metricPeers",connected)','peerConnectedSummary','peerKnownSummary','peerListenerSummary','seen.connected.has(key)||seen.known.has(key)']
missing=[x for x in need if x not in s]
if missing: raise SystemExit('FAIL missing: '+', '.join(missing))
# Regression fixture mirrors the malformed-looking UI case: markers, duplicate known peers and wildcard listeners.
fixture=['[peers]','127.0.0.1:26661','127.0.0.1:26662','127.0.0.1:26663','0.0.0.0:26661','[known]','0.0.0.0:26661','127.0.0.1:26661','127.0.0.1:26662','127.0.0.1:26663']
section='connected'; con=set(); known=set(); listeners=set()
for line in fixture:
    if line=='[peers]': section='connected'; continue
    if line=='[known]': section='known'; continue
    host,port=line.rsplit(':',1); key=(host,port)
    if host in ('0.0.0.0','::','*'): listeners.add(key); continue
    if section=='known':
        if key not in con: known.add(key)
    else:
        con.add(key); known.discard(key)
assert len(con)==3 and len(known)==0 and len(listeners)==1, (con,known,listeners)
print('QRX 0.0.9.76 peer overview audit: PASS')
