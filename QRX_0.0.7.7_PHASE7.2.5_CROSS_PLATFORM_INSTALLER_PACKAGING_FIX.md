# QRX 0.0.7.7 Phase 7.2.5 — Cross-Platform Installer Packaging Fix

Restores the robust installer pipeline from the earlier unified builder while retaining Phase 7.2.4 and the self-contained OpenSSL work.

## macOS x64 / arm64
- Tauri builds the `.app` only (`tauri.macos.conf.json`).
- The verified Cargo GUI executable is installed into `Contents/MacOS/qrx-wallet`.
- `CFBundleExecutable` is forced to `qrx-wallet` with PlistBuddy.
- Bundle gets an ad-hoc local signature after post-bundle modification.
- A Finder/AppleScript-independent DMG is created with `hdiutil` and `/Applications` symlink.
- Build fails if the DMG is absent.

## Linux x64 / arm64
- Tauri targets both `deb` and `appimage`.
- Release fails unless both a `.deb` and `.AppImage` are present.
- Entire Tauri bundle tree is staged into the release ZIP.

## Windows x64
- Restores native MSVC + vcpkg static OpenSSL path.
- `build-windows-x64-static.ps1` installs/uses `openssl:x64-windows-static` and configures QRX explicitly against static libcrypto.
- Tauri targets both MSI and NSIS.
- Release fails unless both `.msi` and NSIS installer `.exe` are present.
- Entire Tauri bundle tree is staged into the release ZIP.

## Release verification
Every target requires Core, CLI, daemon, BTC wallet service, Python tools and at least the complete native installer set before ZIP creation.
