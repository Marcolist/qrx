# QRX 0.0.8.79 — DApp Permission Bridge

The QRX browser remains static and zero-permission by default. DApp mode must be explicitly enabled by the wallet user.

The new core broker binds authorization to the canonical `.qrx` domain, owner, publishing-key commitment and exact site manifest root. Requests are nonce-replay protected, payload bounded and rate bounded. The core supports ONCE, SESSION and PERSISTENT grant records.

The current Wallet DApp bridge exposes only `getAddress`, `getBalance` and `requestPayment`. Verified DApp JavaScript runs in the constrained QRX iframe with `connect-src 'none'`, no generic Tauri IPC and no seed/private-key API. Payment requests always require a second transaction-specific confirmation in the trusted wallet parent.

The GUI currently uses session grants for interactive consent and immediate revoke. Persistent core grant serialization is implemented for the next persistence/settings wiring; no persistent grant is silently created by the browser.

Test status: 54/54 Core CTests pass. Inline Wallet JavaScript syntax passes.
