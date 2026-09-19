# QRX 0.0.9.72 — Human Passphrase UX Hardening

- Adds `qrx-cli walletpassphrase` with a hidden terminal prompt. Users enter their normal wallet passphrase; qrx-cli performs the hex transport conversion internally.
- Rejects `walletpassphrase <secret>` so secrets are not placed in argv/shell history.
- The GUI Command Center recognizes `walletpassphrase` as a local secure alias and uses the existing masked Wallet Security field. The passphrase must never be typed into the command text.
- Legacy encrypted PKCS#8 wallets whose verified passphrase is empty continue to auto-detect and synchronize an empty signer session. No invented password or manual hex conversion is required.
- `walletpassphrasehex`/`walletpassphrasehexfor` remain low-level compatibility commands.
