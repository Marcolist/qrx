#!/usr/bin/env python3
from pathlib import Path
import re, subprocess, sys
root=Path(__file__).resolve().parents[2]
html=root/'GUIWALLET/src/generals/index.html'
s=html.read_text()
checks={
 'phase label':'Phase 7.2.6' in s,
 'reachable hex UX':'.hex.reachable' in s and 'isReachable' in s,
 'route preview':'.move-route' in s and 'addRouteOverlay' in s,
 'commit validation':'Target is not reachable' in s,
 'pending route persistence':'qrx_generals_pending_order' in s and 'previewOrder' in s,
 'reveal resolution':'MOVE REVEALED & RESOLVED' in s and 'GAME_ORDER_REVEAL' in s,
 'demo movement state':'qrx_generals_demo_moves' in s and 'demoApplyMoves' in s,
 'control-click':'e.ctrlKey' in s and "'contextmenu'" in s,
 'music absolute URL':'new URL(tracks[i],document.baseURI).href' in s,
 'music explicit load':'.load()' in s,
 'music error feedback':'Music asset failed to load' in s,
 'next track starts':'function nextMusic(forcePlay=true)' in s,
}
for rel in ['grid-aurora-drift.mp3','grid-relay.mp3','gridfall-charge.mp3','digital-crown.mp3']:
    checks['audio '+rel]=(root/'GUIWALLET/src/generals/audio'/rel).stat().st_size>100000
bad=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(('PASS ' if v else 'FAIL ')+k)
if bad: sys.exit('Failed: '+', '.join(bad))
script=s.split('<script>',1)[1].split('</script>',1)[0]
tmp=Path('/tmp/qrx-generals-726.js'); tmp.write_text(script)
node=subprocess.run(['node','--check',str(tmp)],capture_output=True,text=True)
print('PASS JavaScript syntax' if node.returncode==0 else node.stderr)
raise SystemExit(node.returncode)
