# QRX 0.0.7.7 — Phase 7.2.18.22
## German / French / Spanish Final Linguistic Cleanup

This phase completes the strengthened static-catalog linguistic cleanup for German, French and Spanish.

Changes:
- Removed all remaining mixed-English candidates reported by `scripts/qrx-i18n-lexical-audit.py` for `de`, `fr`, and `es`.
- Rewrote remaining mixed-language wallet, validator, cross-chain, Kraken, privacy, backup/recovery and market text as coherent native-language UI copy.
- Reviewed remaining values that are intentionally identical to English (protocol names, locale self-names, platform names, literals, examples, abbreviations and accepted technical loanwords).
- Regenerated `de-identical-allowlist.json`, `fr-identical-allowlist.json`, and `es-identical-allowlist.json` from the manually reviewed remaining identical values.
- Marked `de`, `fr`, and `es` as `complete` in the editorial manifest only after the stronger lexical gate reached zero candidates and the identical-value allowlists exactly matched the remaining identical keys.
- No consensus, Genesis, staking accounting, wallet key derivation, RPC security, supply, fee or signing behavior changed.

Completion gate after this phase:
- German: complete
- French: complete
- Spanish: complete
- Other locales: remain staged under their existing editorial pass/review status.

Important: `complete` here applies to the 826-key static GUI/Generals catalog covered by the current i18n system and its strict gates. It does not claim that every dynamically generated runtime/daemon error string elsewhere in the product is localized.
