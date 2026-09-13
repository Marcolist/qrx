# QRX 0.0.7.1 — Release Security & Network Hardening

## Security invariants

- GUI-managed wallet JSON-RPC is explicitly bound to `127.0.0.1`.
- Default Core RPC ports remain network-specific: Mainnet 37660, Alpha 37661, Testnet 37662, Regtest 37663.
- A non-loopback `--rpc-bind` is rejected unless the operator explicitly supplies `--allow-remote-rpc` **and** non-empty `--rpc-user` and `--rpc-password`.
- The BTC Light wallet service is a short-lived stdin/stdout sidecar and does not open an inbound TCP server socket.
- QRX P2P networking is intentionally separate from wallet RPC. P2P may listen on `0.0.0.0` when a node is meant to accept peers; this does not expose wallet RPC.
- The old Core Alpha-only legacy-wallet fallback was removed. If a shared wallet is absent but any old per-network wallet exists, Core now fails closed and asks for an explicit copy-only migration instead of silently choosing Alpha or creating a competing identity.

## Why this matters on Wi-Fi/LAN

A normal GUI wallet can communicate with its local `qrxd`, but another computer or phone on the same wireless/LAN network cannot connect to the wallet RPC ports because the socket is bound to loopback only. A firewall remains useful defense in depth, but is no longer the primary mechanism keeping the wallet API off the LAN.

## Remote RPC

Remote RPC is not enabled by the GUI. Server operators must opt in explicitly and provide credentials. Even then, use a VPN/SSH tunnel and host firewall. Plain HTTP RPC should not be exposed directly to the public Internet.

## Verification

Run:

```bash
./qrx-core/scripts/audit-network-security.sh
```

On Linux, a live wallet should show the RPC listener as `127.0.0.1:3766x`, never `0.0.0.0:3766x`.

Useful manual commands:

```bash
ss -lntp | grep -E '37660|37661|37662|37663|qrxd'
```

macOS:

```bash
lsof -nP -iTCP -sTCP:LISTEN | grep -E 'qrxd|37660|37661|37662|37663'
```

Windows PowerShell:

```powershell
Get-NetTCPConnection -State Listen | Where-Object { $_.LocalPort -in 37660,37661,37662,37663 }
```

Expected address: `127.0.0.1`.
