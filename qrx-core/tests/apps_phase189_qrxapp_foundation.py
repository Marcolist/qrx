#!/usr/bin/env python3
import json, pathlib, sys, zipfile
root=pathlib.Path(sys.argv[1]).resolve()
wallet=root/'GUIWALLET'
pkg=wallet/'qrxapp-sdk/dist/hello-qrx.qrxapp'
assert pkg.is_file()
with zipfile.ZipFile(pkg) as z:
    names=z.namelist(); assert 'qrx-app.json' in names
    assert all(not n.startswith('/') and '..' not in pathlib.PurePosixPath(n).parts for n in names)
    m=json.loads(z.read('qrx-app.json'))
assert m['format']==1 and m['sdk']=='1' and m['id']=='org.qrx.demo.hello'
required={'wallet.identity.read','wallet.balance.read','wallet.payment.request','chain.read','network.status.read','app.storage'}
assert required.issubset(set(m['permissions']))
host=(wallet/'src/app-host/index.html').read_text()
assert 'sandbox="allow-scripts"' in host and 'allow-same-origin' not in host
assert "qrx_app_bridge_call" in host and "postMessage" in host
sdk=(wallet/'src/apps/sdk/qrx-sdk.js').read_text()
for method in ['wallet.getIdentity','wallet.getBalance','wallet.requestPayment','chain.getHeight','network.getStatus','storage.get','storage.set']:
    assert method in sdk
rust=(wallet/'src-tauri/src/qrx_apps.rs').read_text()
for token in ['ALLOWED_PERMISSIONS','enclosed_name','Symlinks are not allowed','require_permission','qrx_app_install','qrx_app_register_dev','qrx_app_bridge_call']:
    assert token in rust
assert 'signrawtransactionwithwallet' not in rust and 'sendrawtransaction' not in rust
ui=(wallet/'src/index.html').read_text()
for token in ['Install .qrxapp','Developer Mode','qrx_app_inspect_package','open_qrx_app_window']:
    assert token in ui
road=(root/'QRX_APP_PLATFORM_0.0.9_0.0.10.md').read_text()
assert 'Developer Signature' in road and 'QRX Drive' in road and 'QRX App Directory' in road and '0.0.10' in road
print('PASS: .qrxapp v1 package, registry UI, sandbox host, permission bridge, Mini JS SDK, demo app and 0.0.10 ecosystem boundary')
