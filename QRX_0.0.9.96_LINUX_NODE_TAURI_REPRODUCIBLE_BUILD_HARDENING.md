# QRX 0.0.9.96 — Linux Node/Tauri build hardening

- Linux desktop builds now validate Node.js by version, not only executable presence.
- Node.js 18+ is required before npm/Tauri runs, preventing the late optional-chaining syntax crash seen on Ubuntu 22 hosts with obsolete distro Node.
- Debian/Ubuntu desktop builds bootstrap Node.js 20 LTS through NodeSource when Node is missing/obsolete, then revalidate in-process.
- The GUI Wallet Tauri CLI top-level dependency is pinned exactly to 1.6.0 instead of a caret range.
- Existing `package-lock.json`/`npm-shrinkwrap.json` remains preferred and uses `npm ci`; without a lock, the build clearly reports that transitive npm resolution is not fully reproducible.
- `--node-only` is unchanged and does not bootstrap Node/Tauri.

Security note: bootstrap failure remains fatal; the builder never patches installed Tauri JavaScript to work around an obsolete runtime.
