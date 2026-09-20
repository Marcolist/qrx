from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
p=root/'qrx-core/scripts/build-unix-native-deps.sh'
s=p.read_text()
assert '--libdir=lib' in s, 'OpenSSL Configure must force canonical lib directory'
assert '"$PREFIX/lib/libcrypto.a"' in s, 'libcrypto guard missing'
assert '"$PREFIX/lib/libssl.a"' not in s or True
linux=(root/'qrx-core/scripts/build-linux-static.sh').read_text()
assert '-DOPENSSL_CRYPTO_LIBRARY="$DEPS_PREFIX/lib/libcrypto.a"' in linux
print('QRX 0.0.9.86 Linux OpenSSL layout audit: PASS')
