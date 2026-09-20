# QRX 0.0.9.90 — Linux Tauri ABI + Windows libpng Integrity Hardening

## Windows
- Replaces the ambiguous SourceForge `tar.gz` URL with the canonical versioned `tar.xz` artifact.
- Pins libpng 1.6.58 SHA-256 to `28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775`.
- Cached source archives are verified before reuse; a mismatching cache entry is deleted and downloaded once again, then hard-fails if the digest still differs.
- SHA-256 verification is never bypassed.

## Linux
- Documents and checks the actual split ABI: QRX Browser/Tauri 2 requires WebKitGTK 4.1; GUI Wallet/Tauri 1.6 still requires WebKitGTK 4.0/libsoup2.
- Debian/Ubuntu preflight now checks/installs libsoup2.4-dev and, where the repository provides it, libwebkit2gtk-4.0-dev.
- Verifies libsoup-2.4, javascriptcoregtk-4.0 and webkit2gtk-4.0 before Cargo starts.
- On modern distributions where WebKitGTK 4.0 development files were removed, full GUI build fails early with an explicit migration message instead of failing deep inside Cargo.
- `--node-only` remains independent of GTK/WebKit/Tauri/AI desktop dependencies.

A full GUI Wallet migration from Tauri 1 to Tauri 2 is intentionally not hidden inside a dependency bootstrap patch; it is an application/runtime migration and needs its own compatibility pass.
