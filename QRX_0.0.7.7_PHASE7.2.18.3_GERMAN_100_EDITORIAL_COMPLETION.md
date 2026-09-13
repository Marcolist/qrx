# QRX 0.0.7.7 Phase 7.2.18.3 — German 100% Editorial Completion

Phase 7.2.18.3 completes the German reference-locale editorial pass for the 826-key Wallet + QRX Generals catalog.

## Quality policy

- User-facing prose, actions, warnings, backup/recovery, staking, validator, BTC Light, VELOCITY, Kraken, Privacy, Markets and Generals labels were reviewed in German.
- Protocol/product names and literals remain stable where translating them would reduce correctness: QRX, QUB, Mainnet, BTC Light, VELOCITY, Kraken, BIP157, hashes, addresses, API field names and standard time-range abbreviations.
- Language selector entries intentionally use each language's self-name.
- `de-identical-allowlist.json` records every intentionally English-identical value by translation key.
- `scripts/qrx-i18n-quality-gate.py` is fail-closed: a newly introduced English-identical German string fails unless it is explicitly reviewed and allowlisted.
- Key parity remains mandatory for every supported locale.

This phase changes presentation/localization only. It does not change consensus, wallet key derivation, signing, staking accounting, RPC security, Genesis bytes, or network rules.
