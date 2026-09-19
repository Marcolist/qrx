# QRX 0.0.9.77 — Mainnet Genesis Finalization

- Finalizes exactly 60 unique bootstrap validator addresses.
- Keeps the established allocation at 1,000 QUB per bootstrap validator (60,000 QUB total).
- Finalizes five Ed25519 governance public roots with the existing 3-of-5 threshold.
- No governance private key material is included.
- Changes fresh Core daemon, CLI fallback, GUI Wallet and Tauri command fallbacks to Mainnet by default.
- Alpha, Testnet and Regtest remain explicitly selectable.
- Existing explicit/persisted network selections remain authoritative; the change is the default, not a forced migration.
