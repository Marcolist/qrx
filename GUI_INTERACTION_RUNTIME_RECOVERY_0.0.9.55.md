# QRX 0.0.9.55 — GUI Interaction Runtime Recovery

- Main wallet receives a constrained no-eval bridge for legacy inline actions.
- AURA Window/Large/Close/Send/Clear are explicit controls; Enter sends and Shift+Enter makes a new line.
- Removed the 30-day package UI from integrated AURA.
- QRX Browser controls use explicit listeners. HTTPS WWW navigation no longer depends on qrxd and opens an isolated native Tauri webview. .qrx remains node/resolver-backed.
- QRX Upscaler has a working native picker and local scale/filter/process execution through bundled qrx-upscaler.
- QRX Generals gets a whitelisted delegated bridge for static and dynamically generated buttons.
- New build gate: scripts/audit-gui-interactions.py.
- Consensus/genesis/staking/tokenomics/governance/key semantics unchanged.
