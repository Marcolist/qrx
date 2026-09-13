# QRX App Foundation 0.0.9 — Mini JS SDK v1

QRX 0.0.9 introduces the local application foundation without opening a public app store yet.

## Package format

A `.qrxapp` is a ZIP container with `qrx-app.json` at the root. Format v1 supports a trusted manifest plus a sandboxed HTML/JS/CSS bundle:

```text
my-app.qrxapp
├── qrx-app.json
├── index.html
├── app.js       # optional, declared by manifest
├── app.css      # optional, declared by manifest
└── icon.png     # reserved for UI use
```

Required manifest fields: `format=1`, `id`, `name`, `version`, `author`, `entry`, `sdk=1`. Requested permissions must come from the v1 allowlist.

## Security boundary

Third-party code runs in an iframe with `sandbox="allow-scripts"` and deliberately **without** `allow-same-origin`, forms, popups, top-navigation, camera, microphone, geolocation or direct wallet IPC. The app communicates with a trusted QRX App Host only through `postMessage`. The Rust host re-checks permissions for every SDK call. Apps never receive private keys or signing material.

0.0.9 sideloaded packages are shown as **not signature verified**. Developer signature verification is intentionally a 0.0.10 App Directory feature; a file named `SIGNATURE` does not make a 0.0.9 sideload trusted.

## Mini SDK v1

The SDK is injected automatically; apps do not bundle it.

```js
const identity = await QRX.wallet.getIdentity();
const balance = await QRX.wallet.getBalance();
const height = await QRX.chain.getHeight();
const network = await QRX.network.getStatus();
await QRX.storage.set("key", { hello: "QRX" });
const value = await QRX.storage.get("key");
await QRX.wallet.requestPayment({ recipient: "qrx...", amount: "0.1", memo: "Example" });
```

`requestPayment()` never signs. It only opens/fills the normal wallet Send screen so the user reviews and confirms the transaction.

### Permissions

- `wallet.identity.read`
- `wallet.balance.read`
- `wallet.payment.request`
- `chain.read`
- `network.status.read`
- `app.storage`

App storage is private to the app ID and capped at 64 KiB in the 0.0.9 foundation.

## Demo

`demo-hello/` is the reference app. Build it with:

```bash
./build-qrxapp.sh
```

The result is `dist/demo-hello.qrxapp` (the source tree also ships a ready `hello-qrx.qrxapp`). Install it from **Wallet → Apps → Install .qrxapp**.

## Developer Mode

Enable **Developer Mode** in Wallet → Apps, then choose an unpacked app folder. The folder is loaded live on each app launch/reload but still uses the same sandbox and permission boundary.

## Deliberately deferred to 0.0.10

- developer package signatures and trust chains
- QRX Drive application publishing
- QRX-Net distribution/discovery
- public/decentralized QRX App Directory
- automated update channels
- Local Compute / QRX Compute application capability grants
- app reputation/review metadata
