# QRX 0.0.9.54 GUI Functional Recovery

- Restores explicit first-run/welcome selection; existing wallets are no longer silently auto-opened.
- Welcome Create/Restore now perform real Tauri commands.
- AURA integrated panel is raised above the wallet UI and can open as a separate resizable/fullscreen window.
- Critical toolbar/app buttons use explicit DOM event bindings; IPC absence is fail-visible instead of silently returning `{}`.
- Built-in Generals, Browser and Upscaler open in separate Tauri windows. Their HTML surfaces live under `GUIWALLET/src/` and are included by Tauri `distDir`; qrx-upscaler remains a bundled external binary.
- Resource Globe initializes when its view opens and explains that live regional markers require a running/synced daemon plus privacy-safe atlas data.
- Wallet context exposes RPC endpoint/port to the UI.
- Send exposes a prominent QUB/BTC segmented switch.
- CSS layout recovery replaces fragile 12-column direct-child placement with responsive auto-fit controls and fixes checkline/form overlap.
