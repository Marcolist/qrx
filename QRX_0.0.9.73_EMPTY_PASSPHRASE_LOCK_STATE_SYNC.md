# QRX 0.0.9.73 — Empty-Passphrase & Wallet Lock-State Synchronization

- Daemon signer unlock now cryptographically verifies the supplied passphrase against the wallet Ed25519 private key before marking a session unlocked.
- Empty legacy passphrases remain supported, but only when key decryption actually succeeds.
- Added `walletsessionstatusfor <wallet>` and GUI `wallet_session_status` bridge.
- GUI synchronizes its lock indicator with the daemon signer session instead of relying only on local JavaScript state.
- Wallet names used by signer-session RPC are validated before filesystem resolution.
- Unlock errors no longer always claim `incorrect passphrase` when the underlying failure is different.

This closes the misleading case where CLI could previously report `unlocked:true` after merely storing a passphrase without first proving that it decrypts the key.
