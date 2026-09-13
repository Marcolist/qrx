# QRX 0.0.8.78 — Browser Security Sandbox + Origin Isolation

This step turns the 0.0.8.77 verified fetch path into a zero-permission rendering boundary.

## Security boundary

A QRX page is not trusted merely because its publisher signature is valid. A valid publisher may still publish malicious HTML. Therefore QRX content is rendered only after storage/signature/content verification and then undergoes a second browser-sandbox gate.

`qrx_net_sandbox` accepts only canonical `qrx://<domain.qrx>/<path>` identities. It authorizes no DNS, external network, scripts, wallet IPC, filesystem, popups or downloads. Traversal and alternate schemes fail closed.

The Wallet renderer removes active/privileged HTML elements and inline event handlers, injects a deny-by-default CSP, fetches same-site CSS/images/media/font resources through the already verified QRX fetch API, and rewrites those resources to local `data:` URLs. The final iframe has an opaque origin (`sandbox=""`) and explicit camera/microphone/geolocation/clipboard denial.

This means QRX content has no `file://` requirement and no generic Tauri command surface. Page-initiated navigation is disabled in this phase; the trusted Wallet address bar performs navigation.

## Important scope boundary

0.0.8.78 intentionally does not execute QRX site JavaScript. DApp scripts and wallet interaction begin only in 0.0.8.79 through a narrow origin-bound permission bridge. This is safer than enabling JavaScript first and trying to subtract wallet privileges afterward.

Platform WebViews do not uniformly expose renderer-process control through Tauri 1.x, so this snapshot does not claim universal OS-process isolation. The enforceable boundary here is verified resource protocol semantics + opaque iframe origin + CSP + Permissions Policy + no scripts/IPC.

## Validation

- Core CTest: 53/53 PASS with assertions enabled.
- Release Core compilation: PASS.
- GUI inline JavaScript syntax: PASS.
- Rust/Tauri cargo check: unavailable in the current build environment because Cargo is not installed.
