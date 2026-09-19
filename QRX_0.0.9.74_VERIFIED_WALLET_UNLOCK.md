# QRX 0.0.9.74 — Verified Wallet Unlock

- `qrx-cli walletpassphrase` now targets the selected `--wallet` and calls `walletpassphrasehexfor <wallet> ...`.
- The legacy `walletpassphrasehex` RPC no longer reports an unverified unlock. It cryptographically verifies the active wallet Ed25519 PEM before creating an unlocked signer session.
- `qrx-cli walletlock` now locks the selected wallet session via `walletlockfor`.
- Empty passphrases remain supported only when the encrypted private key actually decrypts with an empty secret.
- Recovery phrases are never accepted as wallet passphrases.
