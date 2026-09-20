# QRX 0.0.9.88 — Headless Node Build + Linux AI Capability Fallback

## Headless node profile

`bash ./scripts/build-all-targets.sh --target host --node-only`

builds and packages the native QRX Core/node stack without desktop-only dependencies. It stages `qrx`, `qrxd`, `qrx-cli`, QRXDB verify/salvage/compact/snapshot tools and the Python wallet CLI helper. It skips Tauri GUI, QRX Browser, BTC GUI service, AURA desktop resources, local AI bundle and desktop installers. The output archive is suffixed `-node-only.zip`.

The node-only profile does not require Cargo/Rust, Node/npm or Linux D-Bus development headers solely for desktop components. Native Core dependencies remain managed by the existing Core build scripts.

## Linux GUI AI fallback

A Linux GUI wallet build no longer fails solely because the optional local Real-ESRGAN/ncnn-Vulkan bundle cannot be prepared on the build host. If AI preparation succeeds, the bundle is still cryptographically verified as before. If it fails on Linux, the builder records `resources/upscaler/AI_UNAVAILABLE.txt` and continues to Tauri; the wallet can be distributed/run with local AI unavailable/pending.

This specifically prevents low-resource/unsupported Linux systems (including ARM boards without a usable Vulkan AI path) from being unable to compile the wallet just because stage 5.5 cannot build the optional AI runtime.

Non-Linux release targets retain the strict verified-AI requirement unless the existing explicit developer opt-out `QRX_ALLOW_AI_PENDING=1` is used.
