# QRX 0.0.8.61 – Drive File UX + Authoritative Shard Route Events

Implemented on top of 0.0.8.60.

## Core / daemon

- Added `qrx_drive_live` authoritative Drive read model backed exclusively by QRXDB storage contracts, assignments and attested provider state.
- Added `listdrivefiles [owner]` RPC/CLI command.
- Added `getdrivefilehealth <contract_id>` RPC/CLI command.
- Added `getdriveshardroutes <contract_id>` RPC/CLI command.
- File health exposes profile, logical size, total/required/healthy/degraded/repairing/missing shards and current reconstructability.
- STANDARD maps to 10 required shards, FAST to replicated-read semantics, ARCHIVE to 16 required data shards when using the planned 16+4 profile.
- Route output never includes provider IP addresses or discovery endpoints.
- Provider regions are exposed only when the existing Resource Atlas privacy threshold is met. Otherwise the route is returned as `privacy-protected` with no region identifier.

## Wallet

- QRX Drive now has a first-class stored-object / contract list instead of a placeholder card.
- Only consensus-backed storage contracts are rendered as stored objects.
- User-visible aliases are local-only and are never inferred from or written to chain state.
- File health is shown per contract.
- Healthy/reconstructable contracts expose a Download action; unhealthy contracts are disabled.
- `Show shards` queries authoritative health + route RPCs and feeds only public-enough regional routes into `qrxSetOwnShardRoutes` for Globe animation.
- Added FAST / STANDARD 10+4 / ARCHIVE upload profile selector and native file preflight picker.
- File preflight explicitly does not claim the file is uploaded or stored before a real storage contract and provider assignments exist.

## Important remaining transport boundary

The Core already contains the 0.0.8.56 parallel multi-provider downloader with hedging/resume and 0.0.8.57 authenticated discovery. However, the daemon does not yet expose a concrete QUIC/provider range-fetch client or upload RPC that connects those abstractions to remote provider sockets. 0.0.8.61 therefore fails closed in the wallet: it does not fabricate a completed network upload/download. A reconstructable Download request reaches save-path + contract-health validation and reports that the daemon-side restore transport bridge is the remaining step.

This separation is intentional: GUI state may never report "stored" or "downloaded" based on mock or local-only data.

## Validation

- Core build: PASS
- Full Core regression suite: 35/35 PASS
- New regression: `storage_phase102_drive_live`
- Wallet inline JavaScript syntax: PASS (`node --check`)
- Tauri `cargo check`: not executed in this environment because Cargo is unavailable.

## Next

0.0.8.62 – Real Provider Transfer Bridge

- QUIC / QRX-P2P range client consuming authenticated 0.0.8.57 discovery endpoints
- daemon-side restore RPC using 0.0.8.56 10-of-14 multi-provider fetch
- upload fan-out to assigned providers with bounded ranges, resume and integrity verification
- persistent transfer journal / crash resume
- progress event stream to wallet
- authoritative upload assignment/accept/proof transitions drive Globe animation
