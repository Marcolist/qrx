# QRX 0.0.7.7 – Generals Serverless Wallet Integration Foundation

This release starts the migration of QRX Generals from its former Node/DB architecture into a native QRX application.

## Implemented in this foundation
- Apps entry in QRX GUI Wallet with Launchpad-style app grid.
- QRX Generals launches in a dedicated Tauri WebView window.
- Only one Generals window is created; repeated launches focus the existing window.
- Generals has its own near-full-window application shell and `Exit to Wallet` control.
- Existing QRX wallet/node process remains the parent application and is not duplicated.
- Existing Generals static art/audio assets are bundled for later client migration.

## Deliberately NOT claimed as complete
The uploaded QRX Generals build still contains authoritative Node.js/database game logic. That server logic is NOT silently embedded or presented as serverless. Before gameplay is enabled it must be migrated into deterministic QRX Core consensus state and transactions.

## Target consensus architecture
- Wallet identity = player identity.
- Transferable General = QRX Unique Asset.
- Block-height seasons and turn windows.
- Commit/reveal orders where hidden simultaneous orders are required.
- Clan creation/membership/leadership as chain state.
- On-chain Generals treasury.
- Atomic season entry payment into treasury.
- Deterministic booster purchases into treasury; no random paid lootboxes by default.
- Energy/cooldowns plus QUB network/anti-spam costs.
- Season reward claims remain reserved in treasury until claimed.
- Optional verified-payout policy through QRX attesters, without storing PII on chain.

This document is an integration foundation, not a statement that the complete serverless game consensus engine has already been security-audited or activated on Mainnet.
