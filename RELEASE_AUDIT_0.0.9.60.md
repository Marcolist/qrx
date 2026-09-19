# QRX 0.0.9.60 release audit — Multi-platform verified AI packaging

Static gates in this source archive:
- complete five-target matrix wired to verified AI bundle staging
- per-platform executable architecture verifier present
- official portable asset map present for macOS/Windows/Linux x64
- pinned source-build path present for Linux ARM64
- common model provenance across all targets
- Linux CI installs Vulkan/glslang build prerequisites
- fail-closed SHA-256 fallback locks retained

Native runtime execution still must be exercised on each corresponding native CI runner/hardware before release signing.
