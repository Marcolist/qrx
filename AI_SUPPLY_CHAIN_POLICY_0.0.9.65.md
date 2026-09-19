# QRX 0.0.9.66 — Local AI Supply-Chain Policy

QRX distribution builds no longer discover or trust AI archive digests at build time.

## Trust boundary

1. The Real-ESRGAN release is pinned to `v0.2.5.0` and exact asset names.
2. Accepted archive SHA-256 values live in `scripts/upscaler-ai-archives.sha256` and are part of the QRX source manifest.
3. macOS additionally pins the canonical archive byte length (51,817,124 bytes).
4. A cache entry is reused only after it matches the committed digest (and pinned size where defined).
5. A mismatching cache is deleted before any network fetch. A mismatching network result fails closed before extraction.
6. ZIP members are checked for path traversal before extraction; executable architecture is checked before staging.
7. The final staged runtime and model files receive their own SHA-256 values in QRX provenance/model manifests.
8. The finished macOS `.app` is re-verified after Tauri packaging.

## Offline/reproducible operation

After one verified fetch, set `QRX_AI_OFFLINE=1`. QRX will use only the verified cache and will fail rather than access the network if the required archive is absent.

The cache can also be supplied by CI/release infrastructure through `QRX_AI_DOWNLOAD_CACHE=/path/to/controlled/cache`.

## macOS v0.2.5.0 lock

Canonical asset: `realesrgan-ncnn-vulkan-20220424-macos.zip`

Expected bytes: `51817124`

SHA-256: `e0ad05580abfeb25f8d8fb55aaf7bedf552c375b5b4d9bd3c8d59764d2cc333a`

The asset identity is pinned to the upstream v0.2.5.0 release. QRX does not query GitHub's API for a digest and does not promote a newly downloaded digest automatically.

## Rotation

Any upstream update requires an explicit QRX source change: new version/asset, reviewed digest, expected metadata, audit update, and a new source manifest. Silent upstream replacement therefore causes a hard failure.
