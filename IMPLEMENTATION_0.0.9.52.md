# QRX 0.0.9.52 — Tauri command registration compile fix

## Fixed

- Restored the missing `#[tauri::command]` annotation on `drive_start_download`, so Tauri generates `__cmd__drive_start_download` for `invoke_handler!` registration.
- Corrected the QRX App window-label normalization from `str::replace('.', '-')` to `str::replace('.', "-")`, matching Rust's `str::replace` replacement argument type.
- Removed the now-unused `std::io::Write` import from `qrx_apps.rs`.

## Scope

This is a GUI Wallet/Tauri compile fix only. It does not change consensus, Genesis, tokenomics, wallet key material, networking rules, or the hermetic native dependency build introduced in 0.0.9.51.

## Validation

- Source inspection confirms `drive_start_download` is now a Tauri command before its `invoke_handler!` registration.
- Source inspection confirms the `replace` call uses a string replacement.
- No remaining identical `replace('.', '-')` pattern exists in the Tauri Rust sources.
- Full `cargo check` could not be executed in the packaging environment because the Rust toolchain is not installed there; native macOS compilation remains the authoritative validation.
