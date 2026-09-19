#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'GUIWALLET/src-tauri/src/main.rs').read_text()
h=(r/'GUIWALLET/src/index.html').read_text()
assert 'enum WalletCliPolicy { ReadOnly, ConfirmOnly, Signing }' in s
assert '"faucet"|"addnode"|"walletlock"|"walletlockfor"|"stop"' in s
assert 'wallet_cli_normalize_arguments' in s
assert 'policy==WalletCliPolicy::Signing' in s
assert 'wallet_cli_policy(normalized[0].as_str())' in s
assert 'only commands that actually sign with wallet keys require an unlocked wallet/passphrase' in h
assert 'getwalletinfo' in s
print('QRX 0.0.9.71 Command-Center policy regression: PASS')
print('read-only=getwalletinfo; confirm-only=faucet; signing commands retain unlock requirement; global CLI options normalized')
