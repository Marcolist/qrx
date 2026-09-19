# QRX 0.0.9.58 — Local AI Super Resolution Runtime Integration

## Scope
0.0.9.58 turns the existing QRX Upscaler app from a classical-only scaler into a fail-closed local AI runtime integration while preserving the deterministic classical fallback.

## Implemented
- `qrx-upscaler ai-status` reports runtime/model/readiness separately.
- `qrx-upscaler verify-model <manifest.qrxmodel>` verifies model `.param` and `.bin` bytes against pinned SHA-256 hashes.
- `qrx-upscaler ai-image <in> <out> --scale 2|4 --tile N` executes a verified local Real-ESRGAN-class model through a `realesrgan-ncnn-vulkan` compatible runtime.
- Apple Silicon path is ncnn Vulkan -> MoltenVK -> Metal. Native QRX Metal inference is not claimed in this phase.
- Auto tile uses the existing hardware/RAM capability profile when `--tile 0` is selected. Apple Silicon uses the RAM-derived recommendation; 8 GiB and above currently selects 512, lower-memory profiles step down.
- Wallet UI exposes AI vs Classical engine, 2x/4x AI scale, Auto/128/256/512 tile selection, model/runtime readiness, model directory and fail-closed controls.
- `ai_ready` is true only when an executable runtime and at least one SHA-256 verified 2x/4x model are present. MoltenVK/Vulkan detection alone is not sufficient.
- Explicit local installer helper accepts a user-supplied runtime + model pair and creates a QRX model manifest. It performs no network download and requires an explicit exact-artifact license/provenance id.

## Security boundaries
- Runtime path from `QRX_UPSCALER_NCNN_RUNTIME` must be absolute.
- Model filenames come only from a local QRX manifest and both files are hashed before every readiness/run decision.
- Runtime invocation uses argv process execution, never `system()` or `popen()`.
- Local AI input is never submitted to QRX Compute.
- Model artifacts are not bundled into this source archive; exact third-party runtime/model bytes remain separately licensed and provenance-checked release artifacts.

## Still open
- Signed QRX runtime/model package catalog and one-click governed downloader.
- Native Metal/CoreML/MLX backend independent of MoltenVK.
- AI video-frame path (current 0.0.9.58 AI command is image-only; classical video remains available).
- Progress/cancel IPC for a running neural job.
- Pi 5, x86 Vulkan and CUDA/P40 production runtime packages and benchmarks.
