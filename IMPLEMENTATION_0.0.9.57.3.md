# QRX 0.0.9.57.3 — Browser Chrome, Privacy Center & WebView Bounds Hardening

## Changes
- Increased compact browser chrome boundary to 128 logical px so website content begins below browser controls.
- Dynamic 244 px browser chrome while the burger/privacy drawer is open; content WebView is moved/resized with it.
- Resize and Retina scale-factor events preserve the current compact/expanded browser chrome geometry.
- Added `+` New Tab control (single active content-WebView semantics for this foundation release).
- Added dedicated Privacy tab/button and local Privacy Center page.
- Added burger menu with explicit WWW / QRX route selection.
- Route mode controls ambiguous address input while explicit `https://`, `qrx://`, and `.qrx` input retains its explicit route.
- Added enforced-policy visibility: HTTPS-only WWW, no QRX-to-DNS fallback, separated browser chrome/content.
- Added native `clear_all_browsing_data()` action for the browser content WebView.
- macOS QRX Browser content WebView now uses a dedicated WKWebView data-store identifier (`QRXBrowserStore1`) to isolate browser website storage from the GUI Wallet on supported macOS versions.
- Tauri identifier remains `org.qrxchain.browser`; GUI Wallet uses its separate identifier.

## Security note
The Privacy Center reports only policies actually enforced by the native host. It does not expose cosmetic toggles for protections that are not implemented.

## Build note
This environment does not provide Rust/Cargo, so Rust compilation must still be validated by the project build on a supported host. Source-level consistency and manifest integrity are checked before packaging.
