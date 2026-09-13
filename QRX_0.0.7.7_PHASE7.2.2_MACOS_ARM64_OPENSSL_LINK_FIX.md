# QRX 0.0.7.7 Phase 7.2.2 – macOS ARM64 OpenSSL Link Fix

Fixes the Apple Silicon release linker failure where QRX Core compiled into libqrxcore.a but qrx/qrxd/qrx-cli failed with unresolved OpenSSL BN_*, EC_*, EVP_*, PEM_*, RAND_* and SHA256 symbols.

Changes:
- qrx-core links crypto dependencies through a dedicated INTERFACE target backed by OpenSSL::Crypto + Threads::Threads.
- all executables retain explicit qrx_crypto_deps linkage via QRX_OPENSSL_LINK_LIBS.
- unified macOS builder resolves a native Homebrew OpenSSL 3 prefix and pins OPENSSL_ROOT_DIR.
- Apple Silicon builds force CMAKE_OSX_ARCHITECTURES=arm64.
- Intel builds force x86_64.
- lipo validates selected libcrypto architecture and final qrx/qrxd/qrx-cli architecture.
- static OpenSSL is requested for release sidecars to avoid accidental runtime dependence on a developer Homebrew dylib.

Validation in release environment:
- shell syntax: PASS
- normal Core/CLI/daemon/QRXDB build: PASS
- PQC-enabled build with internal tests: PASS
- CTest: 7/7 PASS

Recommended Apple Silicon invocation:

    brew install openssl@3
    bash ./scripts/build-all-targets.sh --target macos-arm64

If a custom OpenSSL build is used:

    QRX_OPENSSL_ROOT_DIR=/path/to/native/openssl \
      bash ./scripts/build-all-targets.sh --target macos-arm64
