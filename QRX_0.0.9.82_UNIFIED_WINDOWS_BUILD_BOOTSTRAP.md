# QRX 0.0.9.82 — Unified Windows Build Bootstrap

`build-all-targets.ps1 -Target windows-x64` is the single canonical Windows build pipeline.

- Dependency discovery, known-path recovery, optional winget installation, MSVC/Core, Rust sidecars, AURA/AI staging, Tauri MSI/NSIS, verification and packaging all remain in `build-all-targets.ps1`.
- `build-windows-x64.cmd` contains no build logic. It only asks consent to start one child PowerShell process with `-ExecutionPolicy Bypass`, because a blocked `.ps1` cannot ask for its own policy exception.
- `-InstallDependencies` optionally accepts supported winget dependency installation without the second prompt; default behavior remains interactive Y/N.
- Strawberry Perl is recovered from `C:\Strawberry\perl\bin` and `C:\Strawberry\c\bin` when installed but absent from the current PATH.
- The wrapper does not modify Machine/User execution policy.
