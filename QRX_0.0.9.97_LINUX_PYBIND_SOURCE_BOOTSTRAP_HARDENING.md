# QRX 0.0.9.97 — Linux pybind11 & source-fetch bootstrap hardening

- Clean Ubuntu/Debian GUI builds verify pybind11's Python module and CMake package metadata before expensive build stages.
- Missing metadata is installed from distro packages (`python3-dev`, `python3-pybind11`, `pybind11-dev`) instead of requiring a manual GitHub clone.
- `pybind11_DIR` is exported explicitly for CMake consumers.
- Linux ARM64 Real-ESRGAN source checkout retries the pinned tag up to three times using HTTP/1.1 and never falls back to an unpinned branch.
- `--node-only` remains unaffected.
