# QRX 0.0.9.71 — Command-Center Policy Fix

The desktop Command Center now separates read-only, confirm-only and wallet-signing commands. `faucet` changes test-chain state but does not consume a wallet private key, so it requires explicit confirmation but no wallet passphrase. `getwalletinfo` is read-only. Leading `--network`, `--wallet` and `--datadir` CLI-style global options are normalized before policy classification; GUI-selected network/wallet remain authoritative. Unknown state-changing commands default to the safer signing policy.
