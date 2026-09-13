# QRX 0.0.7.7 Phase 7.2.3 — Self-Contained Static OpenSSL Build Fix

Phase 7.2.3 restores the intended release behavior of the unified builder.

- `scripts/build-all-targets.sh` no longer depends on Homebrew OpenSSL on macOS.
- `qrx-core/scripts/build-macos-static.sh` downloads, SHA-256 verifies and statically builds OpenSSL for the native macOS architecture.
- `qrx-core/scripts/build-linux-static.sh` does the same for Linux x86-64 / ARM64.
- Default OpenSSL is 3.6.4; set `QRX_OPENSSL_VERSION=3.6.2` to reproduce the earlier 3.6.2 dependency.
- OpenSSL is installed under `build/deps/`, not system-wide.
- QRX CMake receives explicit `OPENSSL_ROOT_DIR`, `OPENSSL_INCLUDE_DIR`, and `OPENSSL_CRYPTO_LIBRARY=.../libcrypto.a`.
- macOS verifies final binaries are the requested architecture and have no dynamic libcrypto/libssl dependency.
- Linux verifies no dynamic libcrypto/libssl dependency using `ldd`.

Apple Silicon local build:

```bash
bash ./scripts/build-all-targets.sh --target macos-arm64
```

No `brew install openssl@3` is required.
