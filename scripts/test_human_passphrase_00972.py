#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
c=(r/'qrx-core/src/qrx_cli.c').read_text()
j=(r/'GUIWALLET/src/index.html').read_text()
m=(r/'GUIWALLET/src-tauri/src/main.rs').read_text()
assert 'walletpassphrase (secure interactive prompt)' in c
assert 'qrx_read_hidden_passphrase' in c and '~ECHO' in c and '_getch()' in c
assert 'walletpassphrase takes no argument' in c
assert 'walletpassphrasehex %s\\n", hex[0]?hex:"-"' in c
assert 'if(normalizedUi[ci]==="walletpassphrase")' in j
assert 'Do not type the passphrase in the command.' in j
assert 'passphrase:""' in j and 'encrypted-container-empty-passphrase-no-user-password' in j
assert 'human_unlock_command' in m
print('QRX 0.0.9.72 human passphrase UX audit: PASS')
