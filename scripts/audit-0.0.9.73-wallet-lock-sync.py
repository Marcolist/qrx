from pathlib import Path
r=Path(__file__).resolve().parents[1]
q=(r/'qrx-core/src/qrxd.c').read_text()
c=(r/'qrx-core/src/qrx_cli.c').read_text()
m=(r/'GUIWALLET/src-tauri/src/main.rs').read_text()
h=(r/'GUIWALLET/src/index.html').read_text()
checks={
'daemon verifies secret':'signer_verify_secret(args[1],secret)!=0' in q,
'daemon exposes session status':'walletsessionstatusfor' in q,
'cli routes session status':'walletsessionstatusfor %s' in c,
'Tauri exposes status':'fn wallet_session_status' in m and 'wallet_session_status,' in m,
'GUI synchronizes status':'async function syncWalletSessionState()' in h and 'await syncWalletSessionState();' in h,
'empty passphrase retained':'if(!strcmp(args[2],"-"))' in q or 'strcmp(args[2],"-")' in q,
'wallet path guarded':'signer_wallet_name_safe' in q,
}
bad=[k for k,v in checks.items() if not v]
if bad: raise SystemExit('FAIL: '+', '.join(bad))
print('QRX 0.0.9.73 wallet lock-state sync audit: PASS')
