# QRX 0.0.7.7 — Phase 7.2.18.18 Translation Integrity Hardening

This phase deliberately tightens the definition of "complete".

## Why
The former completion gate only compared locale values with English for exact equality. That detects untranslated copies but cannot detect mechanically partial strings such as mixed English/French or English/German sentences. A new lexical mixed-English audit exposed such legacy fragments, including in catalogs previously labelled complete.

## Changes
- Major reviewed French pass: exact English-identical values reduced from 347 to 107.
- Major reviewed Spanish pass: exact English-identical values reduced from 339 to 96.
- Added `scripts/qrx-i18n-lexical-audit.py`.
- Strengthened `scripts/qrx-i18n-quality-gate.py`: any locale declared `complete` must now pass key parity, exact-identical allowlist parity, and the conservative mixed-English candidate scan.
- Downgraded German from `complete` to `editorial_cleanup_required` after the stronger audit found legacy mixed-language strings.
- French and Spanish remain `editorial_pass_9`; neither is falsely marked complete.

## Scope
No consensus, Genesis, staking accounting, wallet keys, transaction signing, RPC security or protocol behavior changed.
