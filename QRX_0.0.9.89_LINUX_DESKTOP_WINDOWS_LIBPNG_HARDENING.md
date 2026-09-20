# QRX 0.0.9.89 – Linux Desktop + Windows libpng Hardening

## Linux GUI/Tauri bootstrap
Full Linux desktop builds now preflight and, on Debian/Ubuntu, automatically install missing development dependencies for D-Bus, GTK3/GDK/ATK/Cairo, WebKitGTK 4.1, librsvg, Ayatana AppIndicator and patchelf before Cargo/Tauri compilation. `--node-only` remains isolated from these desktop dependencies.

## Windows hermetic libpng
The pinned SHA-256 for the libpng 1.6.58 SourceForge tarball used by the Windows hermetic dependency builder was corrected to:

`f4cc2ac75f181a6e67fb6e25b7e8b5338231fa076019b7e7d4e679f3e619ac36`

Checksum verification remains fail-closed; the build does not accept an arbitrary downloaded archive.
