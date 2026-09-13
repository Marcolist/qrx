# QRX 0.0.9.56 — UI/UX polish

GUI-only pass. Consensus, Genesis, staking, tokenomics, governance and wallet-key semantics are unchanged.

- i18n no longer destroys inline SVG/icon children; Windows/Linux/macOS splash icons remain visible.
- First-start information slides use onboarding revision v3 and appear once for this release.
- Existing-data/welcome screen is top-aligned and scrollable; heading cannot be clipped above the viewport.
- Removed stray literal `html` from QRX Generals.
- Integrated and standalone AURA use a paper-plane Send button. Enter sends; Shift+Enter inserts a new line.
- AURA Local Help now uses safe local wallet/node/RPC/balance context where available and has broader deterministic QRX help.
- Compact icon links added for qrxchain.org, qrxos.com, GitHub, Discord and Bitcointalk.
- Copyright: © 2026 QRX Chain Team.
- QRX Browser keeps ordinary WWW navigation in its content area by default. Sites that block iframe embedding expose an explicit Compatibility View. Tauri 1 cannot host a second arbitrary remote WebView inside the same decorated browser window; true single-window arbitrary WWW requires a later Tauri 2 multi-WebView migration.
