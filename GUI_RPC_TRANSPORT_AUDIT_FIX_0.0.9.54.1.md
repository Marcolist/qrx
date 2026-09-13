# QRX 0.0.9.54.1 — GUI RPC Transport Audit Fix

Fixes the release-blocking GUI/Core compatibility gate triggered by legacy UI terminology.

Changes:
- Sidebar labels the daemon transport as `RPC endpoint:` instead of a control socket.
- Wallet runtime panel labels the value as `RPC endpoint`.
- Locale value for the legacy translation key now represents the RPC endpoint; German uses `RPC-Endpunkt`.
- Daemon startup failure text now says `HTTP RPC endpoint`.
- No consensus, genesis, wallet-key, staking, tokenomics or protocol behavior changed.

Validation:
- `python3 scripts/audit-gui-core-compat.py` PASS
- 86 GUI Core command names validated against qrx-cli dispatch.
- HTTP JSON-RPC `/rpc` ports remain 37660-37663 by network.
