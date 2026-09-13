from pathlib import Path
s=Path('GUIWALLET/src/generals/index.html').read_text()
checks={
 'phase countdown':'id="phaseCountdown"',
 'phase progress':'id="phaseProgress"',
 'commit button wiring':'id="commitMoveBtn"',
 'reveal button wiring':'id="revealMoveBtn"',
 'phase controller':'function updatePhaseUI()',
 'block based real countdown':"leftBlocks*bt-elapsed",
 'sandbox 30s commit/reveal':'DEMO_PHASE_SECONDS=30',
 'boundary refresh':'setTimeout(refreshGame,650)',
 'commit phase guard':"phaseInfo().phase!=='COMMIT'",
 'reveal phase guard':"phaseInfo().phase!=='REVEAL'",
 'stale turn protection':'belongs to an expired turn',
 'one move per turn':'A move is already committed for this turn',
 '7.2.6 movement retained':'function isReachable(',
 '7.2.6 audio retained':'function loadTrack(autoplay=false)',
 'mac control click retained':"if(e.ctrlKey)",
}
failed=[]
for name,needle in checks.items():
    ok=needle in s
    print(('PASS' if ok else 'FAIL'),name)
    if not ok: failed.append(name)
if failed: raise SystemExit('missing: '+', '.join(failed))
