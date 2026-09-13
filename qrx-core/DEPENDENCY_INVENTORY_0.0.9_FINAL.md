# QRX 0.0.9 Final — Dependency Inventory

This is a source-level dependency inventory for release review. It is not a vulnerability scan.

## Native Core

- C/CMake toolchain
- OpenSSL (required by CMake; PQ/hybrid paths require a provider/build exposing the configured algorithms)
- OS threads
- Windows builds additionally link Winsock (`ws2_32`)

## GUI Wallet / Tauri

JavaScript build dependency:
- `@tauri-apps/cli` ^1.6.0

Rust direct dependencies from `GUIWALLET/src-tauri/Cargo.toml`:
- serde 1
- serde_json 1
- tauri 1.6
- dirs 5
- thiserror 1
- bdk 0.29 (`all-keys`, `electrum`)
- base64 0.22
- aes-gcm 0.10
- rand 0.8
- argon2 0.5
- miniscript 10.2.3
- qrcode 0.14
- openssl 0.10

Exact transitive Rust versions are pinned by the shipped Cargo lockfile where present and should be reviewed on each native release runner.

## AURA runtimes/models

Production inference runtimes and model weights are intentionally distributed as separately verified/signed runtime/model artifacts. They are not silently trusted merely because a provider advertises them. 0.0.9.45-0.0.9.48 add package hashing, signed catalog/governance, provenance/license policy and fail-closed readiness checks.
