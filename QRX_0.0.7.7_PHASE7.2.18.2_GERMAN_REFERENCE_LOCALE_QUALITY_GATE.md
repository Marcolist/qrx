# QRX 0.0.7.7 Phase 7.2.18.2 — German Reference Locale & Translation Quality Gate

This phase starts the editorial locale-by-locale pass with German as the terminology reference.

## Rules
- Protocol/product identifiers remain unchanged: QRX, QUB, Mainnet, State Root, TXID, VELOCITY, BTC Light and cryptographic identifiers.
- Safety copy uses explicit German wording and does not soften warnings.
- `de.json` retains exact key parity with `en.json`.
- `scripts/qrx-i18n-quality-gate.py` detects key drift and reports suspicious English-identical strings.
- Runtime fallback remains English so a missing translation can never remove a control.

## Editorial status
The German catalogue has received a broad first editorial pass, with safety-critical onboarding, wallet modes, node health, peer privacy, backup/recovery terminology, market controls and Generals controls explicitly curated. The quality gate intentionally reports remaining English-identical legacy/technical strings instead of pretending they are translated. Those warnings are the queue for the next editorial sweep.
