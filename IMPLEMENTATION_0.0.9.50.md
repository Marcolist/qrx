# QRX 0.0.9.50 — macOS native libpng architecture hardening

## Problem

On Apple Silicon, CMake could discover `/usr/local/lib/libpng.dylib` from an Intel Homebrew installation while the QRX release build was targeting `arm64`. The linker then ignored the x86_64 library and `qrx-upscaler` failed with undefined `png_*` symbols.

## Fix

`qrx-core/scripts/build-macos-static.sh` now:

- selects the Homebrew prefix matching the requested native macOS architecture;
- uses `/opt/homebrew/bin/brew` for `arm64` and `/usr/local/bin/brew` for `x86_64`;
- resolves the corresponding `libpng` prefix;
- verifies the candidate library architecture with `lipo` when available;
- passes `PNG_PNG_INCLUDE_DIR`, `PNG_LIBRARY`, and `CMAKE_PREFIX_PATH` explicitly to CMake;
- refuses to fall back to a libpng of the wrong architecture;
- emits an actionable `brew install libpng` message if the native dependency is unavailable.

This keeps PNG support enabled instead of silently degrading the Upscaler to PPM/PGM-only mode.

## Scope

Build-system only. No consensus, wallet, staking, governance, Genesis, privacy, storage, networking, or tokenomics behavior is changed.
