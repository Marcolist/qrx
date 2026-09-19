# QRX 0.0.9.80 — Native Windows Build Pipeline

The Windows x64 release path is now orchestrated natively by PowerShell. Git Bash/MSYS2 is no longer a prerequisite for `scripts/build-all-targets.ps1 -Target windows-x64`.

## Changes
- Native PowerShell preflight reports all missing prerequisites at once.
- Python discovery supports `python` and the Windows `py -3` launcher.
- Core/CLI/QRXDB uses the existing hermetic MSVC dependency builder.
- Rust BTC service and QRX Browser are built natively for `x86_64-pc-windows-msvc`.
- Target-suffixed Tauri sidecars, MSI and NSIS packaging are orchestrated from PowerShell.
- Windows AI bundle staging has a native PowerShell implementation and retains fail-closed SHA-256 release locks.
- No Git Bash/MSYS2 dependency exists in the Windows x64 path.

## Security note
The reviewed Windows Real-ESRGAN archive SHA-256 is intentionally not invented. If the lock is absent from `scripts/upscaler-ai-archives.sha256`, a normal release build fails closed. A reviewed value can be supplied as `QRX_REAL_ESRGAN_WINDOWS_X64_ARCHIVE_SHA256`. `QRX_ALLOW_AI_PENDING=1` remains an explicit developer-only opt-out.
