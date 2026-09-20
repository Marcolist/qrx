# QRX 0.0.9.85 — Linux Rustup Dependency Bootstrap

- `scripts/build-all-targets.sh` now bootstraps Rust/Cargo automatically on Linux when `cargo` is missing.
- Uses the official rustup installer with its default profile/toolchain and non-interactive acceptance (`-y`).
- Loads `$HOME/.cargo/env` into the active build process immediately, so the same build continues without requiring logout/login.
- On later non-login builds, an existing `$HOME/.cargo/env` is sourced before dependency validation.
- If `curl` is missing on an apt-based Linux host, the script installs `curl` and `ca-certificates` first via `sudo apt-get`.
- macOS and Windows dependency behavior is unchanged.
- The remaining dependency gate still verifies `cargo`, `rustc`, and `rustup` after bootstrap; installation failures remain fatal rather than silently skipping Rust-dependent components.
