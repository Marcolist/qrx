# QRX Chain 0.0.9.60 — Verified AI Packaging for All Desktop Targets

0.0.9.60 extends the 0.0.9.59 Apple-Silicon AI bundle gate to every target produced by `build-all-targets.sh`.

Targets: macOS ARM64, macOS x64, Windows x64, Linux x64, Linux ARM64 / Raspberry Pi 5 class.

Security/provenance rules:
- official Real-ESRGAN v0.2.5.0 release assets only where upstream publishes a portable binary;
- GitHub release digest is required, or a separately reviewed per-asset SHA-256 environment lock is required; no TOFU release packaging;
- archive traversal protection;
- executable header/architecture verification (Mach-O, PE32+ x64, ELF x64/AArch64);
- byte-identical reviewed x2/x4 models on all targets, with `.qrxmodel` v2 SHA-256/provenance;
- runtime SHA-256 is embedded and enforced by the wallet host;
- Linux ARM64 source-build fallback is pinned to Real-ESRGAN-ncnn-vulkan v0.2.0 / `37026f4...` and pinned git submodules.

Acceleration:
- macOS: ncnn Vulkan -> MoltenVK -> Metal;
- Windows x64: native Vulkan;
- Linux x64: native Vulkan, Intel/AMD/NVIDIA; P40 works through NVIDIA's Vulkan driver and does not require a separate CUDA Real-ESRGAN package;
- Linux ARM64/Pi 5: native source build; Vulkan is enabled when the platform driver supports it, but Pi Vulkan remains upstream-described as experimental.
