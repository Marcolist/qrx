# Windows build validation

Validated on Windows x64 with Visual Studio 2022 Build Tools (MSVC 19.44),
CMake 4.4.3, Strawberry Perl and the stable Rust MSVC toolchain.

Run the normal Windows orchestrator from PowerShell:

```powershell
./scripts/build-all-targets.ps1 -Target windows-x64
```

The release build still requires verified AI assets. Until a reviewed Windows
AI archive digest is supplied, the existing developer opt-in can be used:

```powershell
$env:QRX_ALLOW_AI_PENDING = '1'
./scripts/build-all-targets.ps1 -Target windows-x64
```

This opt-in generates a separate Tauri configuration without the missing AI
resource glob; it does not disable verification or modify the release config.
Signed AURA runtime resources must also be configured for a complete release.

The native dependency build, all Core executables, BTC service, QRX Browser and
Tauri wallet were compiled locally. The wallet process reached its `.boot-ok`
marker and remained responsive. QRX Browser also loaded its local home page and
remained responsive. This is a startup smoke test, not a full visual
UI or mainnet synchronization test. The complete orchestrator was not rerun
end-to-end; its individual build stages were exercised during diagnosis.
The MSI and NSIS packages were built with the developer resource configuration.
The binaries require the Windows x64 Visual C++ runtime and WebView2; "static"
in the dependency script refers to the crypto/compression libraries, not every
Windows runtime component. Installer execution on a clean machine is untested.

Regression checks:

```powershell
./scripts/test-windows-cmake-options.ps1
./scripts/test-windows-zlib-artifacts.ps1
# From a Visual Studio developer shell:
python scripts/test-portable-mul-div.py
```

The PowerShell tests pass on both Windows PowerShell 5.1 and PowerShell 7.
The arithmetic check compares 309 cases with Python integer arithmetic,
including out-of-range results. Research acceleration now rejects arithmetic
overflow explicitly instead of narrowing an out-of-range 128-bit quotient.

With `QRX_BUILD_TESTS=ON`, build `qrx_platform_threads_test`,
`qrx_storage_transport_test` and `qrx_storage_multifetch_test`, then run:

```powershell
ctest --test-dir build/core/windows-x64 -C Release -R '^(platform_threads|storage_phase093_transport|storage_phase098_multifetch)$' --output-on-failure
```

All three passed with active checks in Release mode. Additional successful
checks: the six upscaler self-tests, isolated Core hybrid key generation and
wallet inspection, BTC JSON status/error handling against an unreachable local
endpoint, the four GUI/Core/browser audits and the three existing Windows build
audits (00992, 00993, 00995). No payment or mainnet synchronization was tested.
