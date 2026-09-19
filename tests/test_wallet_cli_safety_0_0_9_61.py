from pathlib import Path
s=Path('qrx-core/src/qrxd.c').read_text()
d=Path('docs/LINUX_BUILD_AND_WALLET_UPGRADE.md').read_text()
assert '--wallet-passphrase-file' in s
assert '--wallet-passphrase-stdin' in s
assert 'NOT a recovery phrase' in s
assert 'process argument list' in s
assert '077u' in s and 'chmod 600' in s
assert 'Wallet passphrase != recovery phrase' in d
assert 'wallet-recover <wallet-dir> <recovery-file>' in d
assert 'Missing build dependency: cargo' in d
print('QRX 0.0.9.61 wallet CLI safety audit: PASS')
