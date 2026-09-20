# QRX 0.0.9.83 — Windows Python Launcher Fix

- Fixes PowerShell pipeline unrolling in `Find-Python`: launcher metadata is now returned as one object (`Exe`, `Prefix`) instead of an array.
- Prevents `Run-Python` from treating the first character of a Windows path (for example `C`) as the executable.
- Renames the `Run-Python` argument parameter to `PythonArgs` to avoid ambiguity with PowerShell automatic `$Args`.
- Keeps `build-all-targets.ps1 -Target windows-x64` as the single canonical Windows build pipeline.
- `build-windows-x64.cmd` remains only the opt-in, process-local ExecutionPolicy bootstrap.
