# QRX 0.0.9.81 — Windows Bootstrap Hardening

- Auto-detects Strawberry Perl at `C:\Strawberry\perl\bin` and refreshes common tool paths inside the running process.
- Offers one interactive `winget` installation pass for missing Python, CMake, Rustup, Node.js LTS, Strawberry Perl and Visual Studio 2022 Build Tools/C++ workload.
- Adds `scripts/build-windows-x64.cmd`, which asks Y/N before starting a single PowerShell child with `-ExecutionPolicy Bypass`. It does not modify machine/user policy.
- Keeps `-Plan` non-mutating: it reports missing prerequisites and never installs them.
- Direct hermetic Core builder also recognizes Strawberry Perl when its installer path has not propagated to the shell PATH yet.
