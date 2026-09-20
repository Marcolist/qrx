# QRX 0.0.9.94 — Local Scaler Runtime Capability Gating

- GUI releases always stage and verify the target Real-ESRGAN NCNN runtime and reviewed 2x/4x model files.
- Build-host GPU/Vulkan availability no longer controls release contents.
- `AI_UNAVAILABLE.txt` fallback packaging is removed for normal GUI builds.
- Client capability probing controls whether local AI execution is enabled.
- `qrx-upscaler ai-image` refuses local AI execution when no usable Vulkan/MoltenVK accelerator is detected.
- Classical upscaling remains available.
- `--node-only` remains free of GUI/local-scaler resources.
- Linux ARM64 runtime source checkout is pinned directly to the reviewed tag with recursive shallow submodules and an explicit CMake source-tree integrity check.
