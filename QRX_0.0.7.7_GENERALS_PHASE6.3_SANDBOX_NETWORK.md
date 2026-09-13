# QRX Generals Phase 6.3 — Sandbox Demo & Active Wallet Network

## Implemented
- Generals launch now receives the GUI wallet's active `network` and `wallet` instead of hard-coding `alpha/node1`.
- Generals shows the active network/wallet in the HUD.
- DEMO toggle enters a local, non-value simulation UI with DEMO QUB semantics.
- Demo snapshot includes a populated tactical region, own/enemy units, clan command, ranking, economy, logistics, doctrine and active research.
- Demo actions never call `generals_submit_action`, never sign a transaction and never spend QUB.
- Exit Demo returns to the same selected wallet/network.

## Offline progression audit
QRX Generals time is consensus/block-height based. Seasons, turns, order phases, energy/timers and research completion heights therefore continue while an individual player is offline as long as the QRX network continues producing blocks.

This is NOT yet equivalent to autonomous offline command execution. Actions that require a new signed GAME_* transaction still require a signer. In particular, commit/reveal orders are not automatically revealed by a server when the wallet is offline. This is intentionally not hidden behind a custodial server.

Recommended next hardening: deterministic pre-authorized order queue / pre-signed reveal envelope with strict season+turn+expiry binding, relayed by any node without exposing the wallet private key.
