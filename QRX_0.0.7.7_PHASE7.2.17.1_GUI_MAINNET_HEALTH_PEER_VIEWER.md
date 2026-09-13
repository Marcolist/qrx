# QRX 0.0.7.7 Phase 7.2.17.1 — GUI Mainnet Health & Peer Viewer Integration

This phase wires the Phase 7.2.17 Mainnet Observatory primitives into the native Tauri wallet.

## Mainnet Health dashboard

The Dashboard now contains a compact Mainnet Health card backed by the authenticated local `getmainnethealth` RPC. It shows the node-reported health state, height, blocks behind peer head, connection count, supply invariant result, best block hash, state-root information, protocol status and network. RED/CRITICAL states raise an in-wallet warning banner. On Alpha/Testnet/Regtest the card clearly reports N/A instead of presenting Mainnet data as local-network health.

The GUI does not query a public web service for this information. It talks to the locally authenticated QRX daemon through the existing Tauri/CLI path.

## Local Peer Viewer

`View peers` opens a local-only peer table. The Tauri command calls `getpeerinfo` and falls back to the existing `listpeers` path. The GUI extracts host/domain, port and a simple IPv4/IPv6/domain classification while retaining the raw daemon status line for operator diagnostics.

Peer addresses are not uploaded by this view. This is intentional: a complete public validator/peer IP directory would unnecessarily improve DDoS targeting. qrxscan.com should expose aggregate peer/network diversity metrics rather than this operator-level address table.

## Backend commands

New Tauri commands:

- `get_mainnet_health`
- `get_peer_info`

Both use the existing authenticated `run_cli` path and therefore inherit the RPC token/daemon security model introduced earlier.

## Refresh behavior

Normal Dashboard refresh now refreshes local peer count and, on Mainnet, the health card. The Peer Viewer itself is loaded on demand and can be refreshed independently.

## Security properties

- no external DNS/GeoIP/ASN API is contacted by the GUI;
- no RPC token or wallet secret is rendered;
- peer rows are constructed with DOM `textContent`, not interpolated HTML;
- health is clearly local-node health, not falsely represented as a global oracle;
- existing consensus and wallet behavior are unchanged.
