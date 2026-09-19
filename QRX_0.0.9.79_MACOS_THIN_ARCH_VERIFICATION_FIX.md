# QRX 0.0.9.79 — macOS Thin Architecture Verification Fix

Fixes a false architecture mismatch in `scripts/build-all-targets.sh` seen on Apple Silicon after a successful native arm64 Core build.

The previous release used `lipo -verify_arch` as a hard gate. On the affected macOS/Xcode toolchain a valid thin arm64 Mach-O was reported as a non-fat file and the gate failed even though `lipo -info` confirmed `arm64`.

0.0.9.79 now uses `lipo -archs` and checks the returned architecture list. This accepts both thin single-architecture binaries and universal binaries, while still refusing a package when the requested architecture is absent.
