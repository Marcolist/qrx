# QRX 0.0.9.93 — Windows CMake + Git bootstrap hardening

- Pins Windows native dependency/Core CMake configuration to `Visual Studio 17 2022` + `-A x64`; it no longer inherits `Ninja` from the host/environment while also passing an incompatible platform specification.
- Adds Git for Windows to the unified Windows prerequisite preflight.
- Missing Git can be installed through winget as `Git.Git` together with the other supported dependencies.
- Adds standard Git for Windows locations to the current-process PATH refresh so the same build invocation can continue after installation.
- Keeps the pinned libpng v1.6.58 tag/commit verification from 0.0.9.92.
