# QRX 0.0.9 macOS available-memory build fix

## Problem
`qrx_compute.c` used `_SC_AVPHYS_PAGES` in the macOS branch. macOS does not define this `sysconf()` selector, so Apple Clang stopped with `use of undeclared identifier '_SC_AVPHYS_PAGES'`.

## Fix
The macOS branch now keeps `sysctlbyname("hw.memsize")` for total physical RAM and uses native Mach VM APIs (`host_page_size` + `host_statistics64(HOST_VM_INFO64)`) for available/reclaimable memory. The estimate uses free + inactive pages and is clamped to total RAM.

No consensus, wallet, tokenomics, governance, or protocol behavior is changed. This is host capability detection only.

The earlier unified-build permission hardening is retained: child platform build scripts are invoked through `bash` so ZIP execute-bit loss cannot abort the build.
