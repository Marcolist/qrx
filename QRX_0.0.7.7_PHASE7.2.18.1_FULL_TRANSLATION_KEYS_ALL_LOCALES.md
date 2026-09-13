# QRX 0.0.7.7 Phase 7.2.18.1 — Full Translation-Key Migration

- Wallet and Generals static UI text/placeholder/title/aria-label nodes migrated to stable `data-i18n*` keys.
- 826 catalog keys.
- Locale catalogs for all 24 EU official languages plus Norwegian, Icelandic, Ukrainian, Turkish, Indonesian, Thai, Simplified Chinese, Traditional Chinese and Japanese.
- Runtime locale loading with English fail-safe fallback. Missing catalog files can never blank the UI.
- Globe selector and system-language detection retained.
- Technical identifiers, protocol names, addresses, hashes, QUB/QRX names and code-like values intentionally remain untranslated.
- Dynamic runtime messages continue to use the existing translation helper where keyed; future new UI must add a key instead of raw user-facing strings.

## Release gate
Run the locale audit before packaging. It verifies every static user-facing node has a key and every locale has the complete key set.
