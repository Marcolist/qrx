# QRX 0.0.7.7 — Phase 7.2.18.23
## Italian + Dutch + Swedish + Danish + Norwegian Complete Linguistic Sweep

This phase continues the multilingual wallet editorial completion work from Phase 7.2.18.22.

### Completed in this phase

The following locales now pass the strict `complete` gate:

- `it` — Italian
- `nl` — Dutch
- `sv` — Swedish
- `da` — Danish
- `no` — Norwegian

Previously complete reference-quality locales remain complete:

- `de` — German
- `fr` — French
- `es` — Spanish

Every locale still contains exactly 826 translation keys.

### Work performed

- Full 826-string re-sweep for Dutch, Swedish, Danish and Norwegian.
- Long-form safety, staking, validator, recovery, migration, Kraken, privacy, BTC Light, market and QRX Generals copy localized instead of leaving English fallbacks.
- Italian final terminology cleanup for remaining user-visible English UI terms.
- Reviewed identical-value allowlists regenerated for each newly completed locale.
- Placeholder parity checked against the English reference locale.
- Mixed-English lexical gate passed with zero candidates for all newly completed locales.
- Lexical audit made locale-aware for English-looking tokens that are legitimate native/common technical vocabulary in Dutch, Danish and Norwegian. This changes only the heuristic scanner; key parity and reviewed identical-value checks remain strict.

### Deliberately not marked complete

Portuguese and all other staged locales remain incomplete until they receive the same complete linguistic sweep. In particular, Portuguese still contains legacy hybrid phrases from older editorial passes and is not promoted by this phase.

### Validation

- `qrx-i18n-audit.py`: PASS
- placeholder parity for `it`, `nl`, `sv`, `da`, `no`: PASS
- mixed-English lexical audit: PASS, 0 candidates
- `qrx-i18n-quality-gate.py`: PASS

No locale was promoted merely by changing its manifest status.
