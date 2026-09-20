#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
B="$ROOT/scripts/build-all-targets.sh"
A="$ROOT/scripts/prepare-upscaler-ai-bundle.sh"
grep -q 'python3-pybind11 pybind11-dev' "$B"
grep -q 'export pybind11_DIR=' "$B"
grep -q 'pybind11Config.cmake' "$B"
grep -q 'for attempt in 1 2 3' "$A"
grep -q 'http.version=HTTP/1.1' "$A"
grep -q 'RUNTIME_TAG' "$A"
echo 'QRX 0.0.9.97 Linux pybind/source bootstrap audit: PASS'
