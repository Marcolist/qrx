# QRX Chain 0.0.9.59 — Verified AI Runtime & Model Packaging

0.0.9.59 closes the local-AI packaging gap from 0.0.9.58 for macOS Apple Silicon.

## Pinned upstream
- Runtime/model bundle: official `xinntao/Real-ESRGAN` release `v0.2.5.0`, release commit `685d429`.
- macOS asset: `realesrgan-ncnn-vulkan-20220424-macos.zip`.
- The build does **not** trust a floating `latest` URL. It resolves the named asset from the pinned tag and requires GitHub's release-asset `sha256:` digest before download is accepted.
- The archive is checked for path traversal before extraction. The runtime must contain an `arm64` Mach-O slice.

## Default reviewed models
- 2×: `realesr-animevideov3-x2` (ncnn model, efficient 2× path).
- 4×: `realesrgan-x4plus` (general Real-ESRGAN 4× model).
- `.qrxmodel` v2 manifests pin SHA-256 of `.param` and `.bin` plus upstream URL, release and source archive SHA-256.

## Runtime integrity
The bundled runtime receives its expected SHA-256 from `runtime/runtime.sha256`. `qrx-upscaler` reports `runtime_verified` separately from `runtime_installed`, and `ai_ready` fails closed if the runtime hash mismatches.

## Wallet packaging
`build-all-targets.sh --target macos-arm64` prepares the verified AI bundle before Tauri packaging. The wallet passes absolute packaged resource paths and the expected runtime hash to the `qrx-upscaler` sidecar. No files need to be copied manually after installation.

## Licensing
Real-ESRGAN source is BSD-3-Clause. The official release distributes the model/runtime bundle. The exact upstream license file from the pinned archive is carried into the packaged resources and QRX's third-party notices remain part of source/release documentation. No GFPGAN weights are bundled.

## Deliberate scope
This phase enables the verified macOS Apple-Silicon default bundle. Linux/Windows/P40/CUDA remain separate target-specific bundles rather than silently reusing a macOS runtime.
