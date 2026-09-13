# QRX AURA Runtime Packages — Genesis 0.0.9

A normal user does **not** select CPU/CUDA/Metal, llama.cpp, quantization or a download URL.
`qrxd`/AURA detects the host, selects a compatible signed package, tries the package's SHA3
content root through QRX Drive/CAS first, falls back to the signed HTTPS origin when needed,
verifies SHA3 again after download, probes the plugin ABI and persists the chosen adapter.

The release matrix covers:

- Linux x86-64 CPU
- Linux ARM64 CPU (Pi 5 / ARM SBCs)
- macOS Intel CPU
- macOS Apple Silicon Metal
- Windows x86-64 CPU
- Linux/Windows NVIDIA CUDA, minimum compute capability 6.1 (includes Tesla P40)

The common inference implementation is pinned llama.cpp. The source pin for the Genesis
runtime build is `b10878` by default (override only through an audited release change).
Release CI/build tooling generates the final content roots and signed catalog from the exact
native plugin bytes. Private signing keys are never stored in this repository.

`official-runtime-catalog.qrx` is therefore a **release artifact**, not a hand-edited source
file. The source tree contains the matrix and build/sign scripts needed to reproduce it.
