# QRX 0.0.8.77 — Verified QRX:// Fetch/Cache + WWW/QRX Browser Routing

## Completed

This phase closes the trustworthy read path for PUBLIC_SIGNED QRX-Net sites.

The authoritative chain is now:

`DomainRecord -> web_manifest_root -> active STANDARD storage contract -> >=10 ACTIVE authenticated provider sources -> QRXWEB reconstruction -> QRXNS02 embedded ML-DSA key -> chain key commitment -> ML-DSA site signature -> bundle root -> per-file CAS/content/Merkle verification -> verified cache -> browser`

QRXNS02 embeds only the public ML-DSA key. Private publishing keys never leave the wallet. The on-chain DomainRecord retains only the publishing-key commitment.

The browser cache is content/version scoped by the current domain manifest root. A cached distribution is revalidated before use. If cache verification fails, the entry is not served and a fresh network reconstruction requires at least ten ACTIVE authenticated providers.

`.qrx` and `qrx://` always resolve with `dns_allowed=false`. There is no DNS/WWW fallback after a failed QRX-Net fetch.

The GUI browser shell is intentionally zero-permission for QRX content in this phase. Full active-content isolation and a verified `qrx://` resource protocol belong to 0.0.8.78.

## Validation

- Core CTest: 52/52 PASS
- New regression: `net_phase119_browser_fetch`
- Wallet inline JavaScript: syntax PASS
- Rust/Tauri cargo check: not executed because cargo is unavailable in the build environment
