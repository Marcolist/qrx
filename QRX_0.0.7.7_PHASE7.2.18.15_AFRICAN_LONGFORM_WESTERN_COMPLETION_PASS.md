# QRX 0.0.7.7 — Phase 7.2.18.15

## African Longform + Western Completion Pass

This localization-only phase continues the 55-locale / 826-key catalogue without changing consensus, Genesis, staking accounting, wallet key derivation, or RPC security.

### Editorial work

- Arabic (`ar`), Afrikaans (`af`) and Kiswahili (`sw`) received a manually authored long-form pass across onboarding, non-custodial recovery responsibility, validator/slashing warnings, wallet/delegator mode, local consensus health, peer privacy, receive/send guidance and BTC Light safety.
- French, Spanish, Italian and Portuguese received the remaining obvious backup-verification sentence that was still exactly English.
- Dutch received a larger long-form pass covering wallet identity, local execution, notifications, QRX apps, send checks, BDK/BTC Light, delegation amount semantics, daemon safety, endpoint selection, BTC recovery phrase warnings, public/private keys and legacy passphrases.
- Technical names and protocol literals (QRX, QUB, BTC Light, Quantum Swaps, BDK, `address.txt`, etc.) remain unchanged where appropriate.

### Measured English-identical values after this pass

- fr: 405
- es: 395
- it: 407
- pt: 398
- nl: 479
- ar: 756
- af: 757
- sw: 757

Only German remains `complete`. All other locales retain editorial-pass status until remaining English-identical human-facing strings are reviewed and intentionally translated or explicitly allowlisted.

### Validation

Run:

```bash
python scripts/qrx-i18n-audit.py
python scripts/qrx-i18n-quality-gate.py
```

Expected: 55 locales × 826 keys, exact key parity, and declared editorial-status consistency.
