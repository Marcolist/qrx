# QRX 0.0.9 — macOS static builder permission hardening

Problem: `scripts/build-all-targets.sh --target macos-arm64` invoked `qrx-core/scripts/build-macos-static.sh` directly. In ZIP-based source distributions the child script could lose its executable bit, causing `Permission denied` before compilation began.

Fix:
- invoke Linux/macOS child build scripts explicitly through `bash` from the unified release builder;
- restore executable bits on `build-macos-static.sh` and `build-linux-static.sh` in the packaged source;
- retain direct target/environment forwarding unchanged.

Validation:
- `bash -n scripts/build-all-targets.sh` PASS
- `bash -n qrx-core/scripts/build-macos-static.sh` PASS
- `bash -n qrx-core/scripts/build-linux-static.sh` PASS
- `bash scripts/build-all-targets.sh --target macos-arm64 --plan` PASS

A native Apple-Silicon compile must still be run on macOS; this patch fixes the pre-build permission failure.
