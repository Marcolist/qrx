# QRX 0.0.9.59 release audit — Verified AI packaging

## Completed in source
- Pinned official Real-ESRGAN release: `v0.2.5.0` / release commit `685d429`.
- Pinned macOS universal asset name: `realesrgan-ncnn-vulkan-20220424-macos.zip`.
- HTTPS-only download; GitHub release-asset SHA-256 digest required when available. If GitHub does not expose a digest for this historical asset, release packaging fails closed unless a separately reviewed `QRX_REAL_ESRGAN_ARCHIVE_SHA256` lock is supplied.
- ZIP path-traversal audit before extraction.
- `lipo -archs` gate requires an `arm64` slice for the Apple-Silicon package.
- Reviewed default models staged: `realesr-animevideov3-x2` and `realesrgan-x4plus`.
- `.qrxmodel` v2 includes per-file SHA-256, source URL, pinned release, archive SHA-256 and reviewed license id.
- Runtime binary receives its own SHA-256 lock. `runtime_verified` is independent of `runtime_installed`; `ai_ready` fails closed on runtime tampering.
- Tauri resources include the AI bundle and wallet commands pass packaged absolute paths/hash to the sidecar.
- Embedded resources are covered by the final macOS application code signature when the release app is code-signed/notarized. Future Model-Manager network updates require their own QRX publisher signature and are not conflated with embedded release signing.

## Local audit executed in build container
- `qrx-upscaler` C build: PASS
- classical deterministic selftest: PASS
- qrxmodel-v2 model SHA verification: PASS (controlled fixture)
- runtime SHA verification: PASS (controlled fixture)
- tampered runtime -> `ai_ready=false`: PASS
- AI packaging static audit: PASS
- GUI interaction audit: PASS
- Tauri configuration JSON parse: PASS
- `prepare-upscaler-ai-bundle.sh` shell syntax: PASS

## Native macOS release gate
The final third-party runtime asset cannot be fetched/executed in the isolated build container used to create this source snapshot. On the native Apple-Silicon release host the unified build now performs the upstream download/digest/architecture/model checks before Tauri packaging; any failure aborts the release build.
