# QRX 0.0.9 — Self-Contained Native Dependencies

Release builds no longer use Homebrew, system package-manager or pkg-config copies of native link libraries.

Pinned source-built stack:

- OpenSSL 3.6.4 — upstream release archive + upstream SHA256 sidecar
- zlib 1.3.2 — SHA256 `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`
- libpng 1.6.58 — SHA256 `8c9b05b675ca7301a458df2c2e46f26e1d41ff36b8863f8c33530bc58c2e6225`
- curl/libcurl 8.22.0 — SHA256 `f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7`

The Unix builder downloads each source archive over HTTPS, verifies SHA-256, builds static libraries into a target-specific QRX prefix, and then configures QRX with `QRX_REQUIRE_BUNDLED_DEPS=ON`.

macOS and Linux release binaries are rejected if they dynamically reference OpenSSL, zlib, libpng or libcurl. macOS also rejects `/usr/local` and `/opt/homebrew` runtime references.

Build tools such as the OS compiler, CMake, make, Perl, Python, Rust/Cargo and Node/npm remain host build prerequisites. They are not QRX runtime link dependencies.

Rust dependencies are compiled from source by Cargo using the repository lockfile. JavaScript/Tauri frontend dependencies should be installed with `npm ci` against `package-lock.json` where present.
