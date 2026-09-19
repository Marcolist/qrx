# QRX 0.0.9.62 — Verified AI Packaging / Network Resilience

- Removed dependency on GitHub API `digest` metadata for Real-ESRGAN packaging.
- Added repository SHA-256 lock file (`scripts/upscaler-ai-archives.sha256`).
- Added persistent verified download cache (`.cache/qrx-ai`, override with `QRX_AI_DOWNLOAD_CACHE`).
- Downloads use retries, retry-all-errors, connect timeout and total timeout.
- Cache entries are SHA-256 checked before reuse; corrupt/stale entries are discarded.
- No TOFU: unknown archive digests remain fail-closed.
- Development builds may continue when a verified AI bundle cannot be staged; AI remains pending.
- Release builds set `QRX_REQUIRE_VERIFIED_AI=1` and fail closed unless every required archive has a reviewed lock.
- macOS v0.2.5.0 uses the official upstream universal macOS portable runtime; the Ubuntu archive is only the canonical model source. It is not executed on macOS.
