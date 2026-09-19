# QRX Upscaler AI model package v1

0.0.9.58 adds a fail-closed local AI adapter for a `realesrgan-ncnn-vulkan` compatible runtime.
The runtime is executed locally using argv-only process spawning; QRX never uploads input media in local mode.

A model is considered usable only when its `.qrxmodel` manifest parses and both the `.param` and `.bin` files match their pinned SHA-256 hashes.

```text
format=qrx-upscaler-model-v1
id=<runtime model name>
scale=2|4
param=<relative .param file>
weights=<relative .bin file>
param_sha256=<64 lowercase hex>
weights_sha256=<64 lowercase hex>
license_id=<reviewed exact artifact license/provenance id>
```

Default macOS paths:
- runtime: `~/Library/Application Support/QRX/upscaler/runtime/realesrgan-ncnn-vulkan`
- models: `~/Library/Application Support/QRX/upscaler/models`

The helper `scripts/install-upscaler-ai-local.sh` copies an explicitly supplied runtime/model pair and generates the integrity manifest. It does not download third-party executables or weights and does not claim a license on their behalf.

Apple Silicon backend in this phase: ncnn Vulkan -> MoltenVK -> Metal. Native QRX Metal inference remains a later backend, not a synonym for the current ncnn adapter.


## v2 (0.0.9.59)
Release packaging uses `qrx-upscaler-model-v2`. In addition to the v1 integrity fields it requires:

```text
provenance_source_url=<canonical upstream project URL>
provenance_release=<pinned release/tag and commit reference>
provenance_archive_sha256=<64 lowercase hex>
```

For the verified macOS Apple-Silicon default bundle, the wallet also carries `runtime/runtime.sha256`. The GUI host supplies that expected hash to the sidecar via `QRX_UPSCALER_RUNTIME_SHA256`; a runtime hash mismatch makes `runtime_verified=false` and `ai_ready=false`.
