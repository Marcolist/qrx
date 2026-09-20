# QRX 0.0.9.87 — AURA + Linux D-Bus Build Hardening

- Replaced the fragile GUI polish audit of the literal Unicode `➤` glyph with semantic checks for the AURA send button, `ask()` handler and click binding. This avoids Windows code-page dependent false negatives while still failing if the send interaction is actually removed.
- All GUI polish audit HTML reads now explicitly decode UTF-8.
- Linux Ubuntu/Debian build bootstrap now detects `pkg-config` and `dbus-1 >= 1.6`; missing `pkg-config` / `libdbus-1-dev` are installed non-interactively through apt before Rust/Tauri compilation.
- The bootstrap verifies D-Bus pkg-config metadata after installation and fails closed if it is still unavailable.
