param(
  [string]$VcpkgRoot = $env:VCPKG_ROOT,
  [string]$BuildDir = "",
  [int]$Jobs = 0
)
$ErrorActionPreference = "Stop"
$Core = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $Core "build-windows-x64-static" }
if (-not $VcpkgRoot) {
  if ($env:VCPKG_INSTALLATION_ROOT) { $VcpkgRoot = $env:VCPKG_INSTALLATION_ROOT }
  else { $VcpkgRoot = "C:\vcpkg" }
}
$Vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $Vcpkg)) { throw "vcpkg.exe not found at $Vcpkg. Set VCPKG_ROOT." }
if (-not (Test-Path $Toolchain)) { throw "vcpkg toolchain missing at $Toolchain" }
& $Vcpkg install "openssl:x64-windows-static"
if ($LASTEXITCODE -ne 0) { throw "vcpkg OpenSSL install failed" }
$Installed = Join-Path $VcpkgRoot "installed\x64-windows-static"
$Crypto = Join-Path $Installed "lib\libcrypto.lib"
if (-not (Test-Path $Crypto)) {
  $Crypto = Join-Path $Installed "lib\crypto.lib"
}
if (-not (Test-Path $Crypto)) { throw "static OpenSSL crypto library not found under $Installed\lib" }
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
$cmakeArgs = @(
  "-S", $Core,
  "-B", $BuildDir,
  "-A", "x64",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
  "-DVCPKG_TARGET_TRIPLET=x64-windows-static",
  "-DOPENSSL_ROOT_DIR=$Installed",
  "-DOPENSSL_USE_STATIC_LIBS=TRUE",
  "-DOPENSSL_CRYPTO_LIBRARY=$Crypto",
  "-DQRX_REQUIRE_PQC=ON"
)
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
$buildArgs = @("--build", $BuildDir, "--config", "Release")
if ($Jobs -gt 0) { $buildArgs += @("--parallel", "$Jobs") }
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
$expected = @("qrx.exe","qrx-cli.exe","qrxd.exe","qrxdb_verify.exe","qrxdb_salvage.exe","qrxdb_compact.exe","qrxdb_snapshot.exe")
foreach ($name in $expected) {
  $p = Join-Path (Join-Path $BuildDir "Release") $name
  if (-not (Test-Path $p)) { throw "Expected Windows artifact missing: $p" }
}
Write-Host "Static Windows x64 QRX build complete: $BuildDir\Release"
