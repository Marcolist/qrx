# QRX 0.0.9.57 — Isolated Tauri 2 Multi-WebView Browser

The wallet remains on Tauri 1. The QRX Browser is now an independent Tauri 2 executable bundled as a wallet sidecar.

Architecture:
- one native QRX Browser window
- child WebView `browser-chrome` at the top (address bar, back, forward, reload, home)
- child WebView `browser-content` below it
- child bounds follow parent resize/scale-factor changes
- normal WWW URLs navigate the native content WebView directly
- plain HTTP is rejected; HTTPS only
- .qrx / qrx:// keeps the existing qrx-cli `resolvebrowserinput` + `fetchqrxsite` verified-cache path and never falls back to DNS
- browser process receives the selected QRX network and wallet from the GUI Wallet
- old Tauri 1 compatibility viewer is no longer used by the Apps launcher

Build:
- Tauri 2.11.5 is pinned for the isolated browser host
- tauri-build 2.6.3 is pinned
- Cargo.lock is generated once if absent, then the build itself runs with --locked
- qrx-browser is target-suffixed and bundled via the existing GUI Wallet externalBin mechanism

No consensus, Genesis, tokenomics, staking, governance, wallet-key or QRX protocol semantics were changed.
