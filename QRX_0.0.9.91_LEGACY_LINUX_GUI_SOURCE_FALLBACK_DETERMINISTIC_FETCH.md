# QRX 0.0.9.91

## Legacy Linux GUI source fallback
- Keeps the existing Tauri 1 GUI wallet build path.
- Uses distro WebKitGTK 4.0/libsoup2 when present.
- If missing, builds private pinned libsoup 2.74.3 + WebKitGTK 2.44.4 under `build/deps/<target>/webkit4`.
- Source archives are SHA-256 pinned; no legacy libraries are installed into `/usr`.
- WebKitGTK is configured for GTK3 + libsoup2 (`webkit2gtk-4.0`).
- `--node-only` remains independent of desktop dependencies.

## Linux packaging
- DEB is built first and is authoritative.
- AppImage is attempted separately; AppImage tooling failure no longer discards a successful wallet/DEB build.

## Windows libpng integrity
- Official libpng 1.6.58 archive SHA-256 remains `28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775`.
- A SourceForge response with any other digest is rejected.
- If the canonical archive fetch cannot produce the published digest, the builder falls back to the official pnggroup/libpng Git repository and verifies the v1.6.58 tag resolves to pinned commit `3061454d980de7d53608f594194cfac722721d2a` before building.
- Integrity verification is never disabled.
