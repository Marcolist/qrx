# QRX 0.0.8.59 – Live Resource RPC + Wallet Wiring

Implemented on top of 0.0.8.58.

## Core
- Added `qrx_resource_live_snapshot()` as the authoritative QRXDB -> dashboard/atlas aggregation path.
- Reads live `storage/provider/`, `storage/contract/`, `storage/assignment/` and `qrxnet/domain/` state.
- Uses STANDARD 10+4 capacity semantics for network usable-capacity calculations.
- Does not fabricate PUBLIC_SIGNED request/cache telemetry; those counters remain zero until authoritative serving telemetry lands.
- Preserves Globe privacy: only atlas cells meeting the minimum provider threshold are exposed by the public atlas RPC.

## Daemon / CLI RPC
- `getresourcedashboard`
- `getresourceatlas`
- `qrx-cli` exposes both commands.

`getresourceatlas` uses `privacy_min_providers = 3` and does not return region names/details for hidden cells.

## Wallet
- Added Tauri commands `resource_dashboard_snapshot` and `resource_atlas_snapshot`.
- QRX Drive consumes the live daemon dashboard RPC.
- Resource Globe foundation consumes the privacy-filtered atlas RPC.
- UI no longer reports the live daemon RPC as missing.

## Validation
- Added `storage_phase101_live_dashboard`.
- Test seeds authoritative QRXDB provider/contract/assignment/domain state and verifies live aggregation and privacy filtering.
- Full Core CTest suite: **34/34 passed**.
- C core/daemon/CLI build completed successfully.
- Rust/Tauri `cargo check` could not be executed in the current build container because Cargo is not installed; the Rust changes are limited to two commands using the already existing `run_cli`/`Value` patterns.
