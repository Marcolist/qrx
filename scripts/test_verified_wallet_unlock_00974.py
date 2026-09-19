from pathlib import Path
r=Path(__file__).resolve().parents[1]
c=(r/'qrx-core/src/qrx_cli.c').read_text()
d=(r/'qrx-core/src/qrxd.c').read_text()
assert 'walletpassphrasehexfor %s %s\\n", wallet, hex[0]?hex:"-"' in c
assert 'walletlockfor %s\\n",wallet' in c
assert 'qrx_set_env("QRX_PASSPHRASE", secret, 1)' not in d[d.index('if(!strcmp(args[0], "walletpassphrasehex")'):d.index('if(!strcmp(args[0], "walletlock")')]
assert 'signer_verify_secret(active_wallet,secret)' in d
assert 'verified\\":true' in d
print('QRX 0.0.9.74 verified wallet unlock regression: PASS')
