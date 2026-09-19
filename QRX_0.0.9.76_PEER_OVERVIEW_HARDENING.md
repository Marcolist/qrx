# QRX 0.0.9.76 — Peer Overview Hardening

The GUI peer viewer now interprets the daemon's `list-peers` sectioned output instead of rendering every raw line as a peer.

- `[peers]` and `[known]` are section markers, never peer rows.
- Wildcard bind addresses (`0.0.0.0`, `::`, `*`) are local listeners and are never counted as connected peers.
- Connected peers are de-duplicated by host/port.
- Known/seed endpoints already present in the connected set are not shown twice.
- Dashboard `Peers` counts only unique connected remote endpoints.
- Viewer shows separate Connected, Known / Seeds, and Local listener groups and counters.
- Raw endpoint lines remain visible for diagnostics.

This is a GUI interpretation/UX hardening change; it does not alter P2P consensus or peer selection.
