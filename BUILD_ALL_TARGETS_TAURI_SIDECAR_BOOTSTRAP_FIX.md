# QRX 0.0.7 unified compiler — Tauri sidecar bootstrap fix

The shared BTC wallet service lives in its own Cargo package at `GUIWALLET/btc-wallet-service`. The unified release builder compiles that package first, then stages the four exact target-suffixed sidecars (`qrx`, `qrx-cli`, `qrxd`, `qrx-btc-wallet-service`) into `GUIWALLET/src-tauri/bin/`. Only after all sidecars exist does it invoke the Tauri 1.x application build.

`GUIWALLET/src-tauri/build.rs` calls `tauri_build::build()` for the actual desktop application. Because the BTC service is a separate Cargo package, its preliminary compilation no longer executes the Tauri application build script and therefore cannot trigger premature `externalBin` validation.
