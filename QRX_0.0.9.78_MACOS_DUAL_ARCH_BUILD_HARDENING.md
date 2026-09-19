# QRX 0.0.9.78 — macOS Dual-Architecture Build Hardening

- Adds `--target macos-both` to build arm64 and x86_64 releases sequentially on macOS.
- Apple Silicon may cross-build the Intel x86_64 target; Linux/Windows native-runner restrictions are unchanged.
- Reuses QRX's pinned, source-built OpenSSL dependency prefix for Rust `openssl-sys` instead of host pkg-config/Homebrew.
- Exports Cargo target-specific OpenSSL variables for x86_64 and arm64 macOS builds.
- Requires both static `libcrypto.a` and `libssl.a` plus OpenSSL headers before Rust/Tauri compilation.
- Verifies every macOS Core/CLI binary with `lipo -verify_arch` before it can be staged into the GUI Wallet.
- Core, Cargo/Tauri and release output directories remain isolated per architecture.

On an M1/M2/M3/M4 Mac:

```bash
bash scripts/build-all-targets.sh --target macos-both
```

Intel only:

```bash
bash scripts/build-all-targets.sh --target macos-x64
```

Outputs remain under `dist/macos-arm64/` and `dist/macos-x64/`.
