# QRX 0.0.7.7 Phase 7.2.18 — Beginner UX, Multilingual Wallet & Backup Safety Center

Phase 7.2.18 is a wallet-safety/comfort release. It does not alter consensus.

## Added
- Global globe language switcher with system-language detection, explicit locale persistence and English fallback.
- Locale registry covering EU official languages plus Norwegian, Icelandic, Ukrainian, Turkish, Indonesian, Thai, Simplified Chinese, Traditional Chinese and Japanese.
- Simple/Advanced experience switch. Dangerous/expert wallet-dump controls are hidden in Simple mode.
- Safety Center with readable wallet/node/protocol/supply/backup checks and a protection score.
- Backup Center exposing existing copy-only encrypted wallet-directory backup and recovery phrase + `.qrxseed` workflow.
- Expert full-wallet dump is deliberately the encrypted/copy-only wallet directory. Plaintext private-key dumping is not made a beginner operation.
- Backup Health verification guidance; destructive restore is never used for verification. Restore to a new wallet name remains the safe recovery test.
- Network Guard confirmation when entering/leaving Mainnet.
- Going-offline guidance routes validator operators to existing consensus-native Safe Pause.
- Existing Mainnet Health + Peer Viewer remains intact.

## Localization safety
The UI uses stable translation keys and a fallback chain: selected locale -> English -> key. A missing translation therefore cannot remove a control or silently change consensus behavior. Cryptographic identifiers, addresses and protocol constants are never translated.

## Important scope note
The locale framework and language selector cover the requested language set. Phase 7.2.18 seeds critical Safety Center translations and establishes the full catalog architecture; the very large legacy wallet UI still contains English strings that fall back safely to English until their individual keys are migrated. This is intentional rather than pretending every historical sentence was professionally translated.
