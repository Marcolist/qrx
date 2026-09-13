# QRX 0.0.9.51 — Hermetic Native Dependency Stack

Release-native dependencies are now source-built and target-pinned instead of being taken from Homebrew, vcpkg, pkg-config or arbitrary system prefixes.

Default pinned native stack:
- OpenSSL 3.6.4 (verified with upstream SHA256 sidecar)
- zlib 1.3.2
- libpng 1.6.58
- curl/libcurl 8.22.0

macOS arm64/x86_64 and Linux arm64/x86_64 use `qrx-core/scripts/build-unix-native-deps.sh`. Windows x64 builds the same native dependency set from source using MSVC and the OpenSSL Windows build system.

`QRX_REQUIRE_BUNDLED_DEPS=ON` makes the CMake release configure fail closed if native dependencies resolve outside `QRX_DEPS_PREFIX`.

macOS/Linux post-link gates reject dynamic OpenSSL/libpng/libcurl/zlib dependencies. macOS additionally rejects Homebrew `/usr/local` and `/opt/homebrew` runtime links.

Host build tools remain prerequisites; they are not release runtime dependencies. Cargo dependencies are source-compiled by Rust. Frontend dependencies are still managed by npm/Tauri and should be lockfile-driven for deterministic release builds.
