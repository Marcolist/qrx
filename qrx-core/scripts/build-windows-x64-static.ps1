param(
  [string]$BuildDir = "",
  [int]$Jobs = 0,
  [string]$DepsPrefix = ""
)
$ErrorActionPreference = "Stop"
# Strawberry Perl may be installed correctly while the current shell still has an old PATH.
foreach($p in @("C:\Strawberry\perl\bin","C:\Strawberry\c\bin")){
  if((Test-Path $p) -and (($env:Path -split ';') -notcontains $p)){ $env:Path="$p;$env:Path" }
}
$Core = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Repo = (Resolve-Path (Join-Path $Core "..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $Repo "build\core\windows-x64" }
if (-not $DepsPrefix) { $DepsPrefix = Join-Path $Repo "build\deps\windows-x64" }
if ($Jobs -le 0) { $Jobs = [Environment]::ProcessorCount }
$SourceCache = Join-Path $Repo "build\deps\sources"
$Work = Join-Path $Repo "build\deps\work\windows-x64"
New-Item -ItemType Directory -Force -Path $DepsPrefix,$SourceCache,$Work | Out-Null

$OpenSSLVersion = if ($env:QRX_OPENSSL_VERSION) { $env:QRX_OPENSSL_VERSION } else { "3.6.4" }
$ZlibVersion = if ($env:QRX_ZLIB_VERSION) { $env:QRX_ZLIB_VERSION } else { "1.3.2" }
$PngVersion = if ($env:QRX_LIBPNG_VERSION) { $env:QRX_LIBPNG_VERSION } else { "1.6.58" }
$CurlVersion = if ($env:QRX_CURL_VERSION) { $env:QRX_CURL_VERSION } else { "8.22.0" }
$ZlibSha = "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
$PngSha = "8c9b05b675ca7301a458df2c2e46f26e1d41ff36b8863f8c33530bc58c2e6225"
$CurlSha = "f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7"

function Need([string]$Name) { if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) { throw "Missing build tool: $Name" } }
foreach ($c in @("cmake","perl","tar")) { Need $c }

function Fetch([string]$Url,[string]$Out) {
  if (-not (Test-Path $Out) -or (Get-Item $Out).Length -eq 0) {
    Write-Host "Downloading $Url"
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile "$Out.tmp"
    Move-Item -Force "$Out.tmp" $Out
  }
}
function Verify([string]$File,[string]$Expected) {
  $got=(Get-FileHash -Algorithm SHA256 $File).Hash.ToLowerInvariant()
  if ($got -ne $Expected.ToLowerInvariant()) { throw "SHA256 mismatch for $File`nexpected $Expected`nactual   $got" }
}
function Extract([string]$Archive,[string]$Destination) {
  if (Test-Path $Destination) { Remove-Item -Recurse -Force $Destination }
  New-Item -ItemType Directory -Force -Path $Destination | Out-Null
  & tar -xf $Archive --strip-components=1 -C $Destination
  if ($LASTEXITCODE -ne 0) { throw "Failed to extract $Archive" }
}

$OsslTar=Join-Path $SourceCache "openssl-$OpenSSLVersion.tar.gz"
$OsslShaFile="$OsslTar.sha256"
Fetch "https://github.com/openssl/openssl/releases/download/openssl-$OpenSSLVersion/openssl-$OpenSSLVersion.tar.gz" $OsslTar
Fetch "https://github.com/openssl/openssl/releases/download/openssl-$OpenSSLVersion/openssl-$OpenSSLVersion.tar.gz.sha256" $OsslShaFile
$OsslExpected=((Get-Content $OsslShaFile | Select-Object -First 1) -split '\s+')[0]
if ($OsslExpected -notmatch '^[0-9a-fA-F]{64}$') { throw "Invalid OpenSSL checksum sidecar" }
Verify $OsslTar $OsslExpected
$ZlibTar=Join-Path $SourceCache "zlib-$ZlibVersion.tar.gz"; Fetch "https://zlib.net/fossils/zlib-$ZlibVersion.tar.gz" $ZlibTar; Verify $ZlibTar $ZlibSha
$PngTar=Join-Path $SourceCache "libpng-$PngVersion.tar.gz"; Fetch "https://download.sourceforge.net/libpng/libpng-$PngVersion.tar.gz" $PngTar; Verify $PngTar $PngSha
$CurlTar=Join-Path $SourceCache "curl-$CurlVersion.tar.xz"; Fetch "https://curl.se/download/curl-$CurlVersion.tar.xz" $CurlTar; Verify $CurlTar $CurlSha

# OpenSSL's Windows build requires the MSVC developer environment. Locate it
# without depending on vcpkg/Chocolatey/Homebrew-like package managers.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio vswhere.exe not found" }
$vsroot = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if (-not $vsroot) { throw "Visual Studio C++ toolchain not found" }
$vcvars = Join-Path $vsroot "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }

$Crypto=Join-Path $DepsPrefix "lib\libcrypto.lib"
if (-not (Test-Path $Crypto)) {
  $src=Join-Path $Work "openssl-$OpenSSLVersion"; Extract $OsslTar $src
  $cmd='"{0}" && cd /d "{1}" && perl Configure VC-WIN64A no-shared no-tests no-asm --prefix="{2}" --openssldir="{2}\ssl" && nmake && nmake install_sw' -f $vcvars,$src,$DepsPrefix
  & cmd.exe /d /s /c $cmd
  if ($LASTEXITCODE -ne 0) { throw "OpenSSL source build failed" }
}
if (-not (Test-Path $Crypto)) { $Crypto=Join-Path $DepsPrefix "lib\crypto.lib" }
if (-not (Test-Path $Crypto)) { throw "Static OpenSSL crypto library missing" }

function CMakeInstall([string]$Source,[string]$Build,[string[]]$Args) {
  if (Test-Path $Build) { Remove-Item -Recurse -Force $Build }
  & cmake -S $Source -B $Build -A x64 @Args
  if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $Source" }
  & cmake --build $Build --config Release --parallel $Jobs
  if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $Source" }
  & cmake --install $Build --config Release
  if ($LASTEXITCODE -ne 0) { throw "CMake install failed: $Source" }
}

$ZlibStatic=Join-Path $DepsPrefix "lib\zlibstatic.lib"
if (-not (Test-Path $ZlibStatic)) {
  $src=Join-Path $Work "zlib-$ZlibVersion"; Extract $ZlibTar $src
  CMakeInstall $src (Join-Path $Work "zlib-build") @("-DCMAKE_BUILD_TYPE=Release","-DCMAKE_INSTALL_PREFIX=$DepsPrefix","-DBUILD_SHARED_LIBS=OFF","-DZLIB_BUILD_TESTING=OFF")
}
if (-not (Test-Path $ZlibStatic)) { $ZlibStatic=Join-Path $DepsPrefix "lib\zlib.lib" }
if (-not (Test-Path $ZlibStatic)) { throw "Static zlib missing" }

$PngStatic=(Get-ChildItem (Join-Path $DepsPrefix "lib") -Filter "*png*static*.lib" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $PngStatic) {
  $src=Join-Path $Work "libpng-$PngVersion"; Extract $PngTar $src
  CMakeInstall $src (Join-Path $Work "libpng-build") @("-DCMAKE_BUILD_TYPE=Release","-DCMAKE_INSTALL_PREFIX=$DepsPrefix","-DBUILD_SHARED_LIBS=OFF","-DPNG_SHARED=OFF","-DPNG_STATIC=ON","-DPNG_TESTS=OFF","-DPNG_TOOLS=OFF","-DZLIB_ROOT=$DepsPrefix","-DZLIB_LIBRARY=$ZlibStatic","-DZLIB_INCLUDE_DIR=$DepsPrefix\include")
  $PngStatic=(Get-ChildItem (Join-Path $DepsPrefix "lib") -Filter "*png*.lib" | Where-Object { $_.Name -notmatch 'dll' } | Select-Object -First 1).FullName
}
if (-not $PngStatic) { throw "Static libpng missing" }

$CurlStatic=Join-Path $DepsPrefix "lib\libcurl.lib"
if (-not (Test-Path $CurlStatic)) {
  $src=Join-Path $Work "curl-$CurlVersion"; Extract $CurlTar $src
  CMakeInstall $src (Join-Path $Work "curl-build") @(
    "-DCMAKE_BUILD_TYPE=Release","-DCMAKE_INSTALL_PREFIX=$DepsPrefix","-DBUILD_SHARED_LIBS=OFF","-DBUILD_CURL_EXE=OFF","-DBUILD_TESTING=OFF",
    "-DCURL_USE_OPENSSL=ON","-DCURL_ZLIB=ON","-DOPENSSL_ROOT_DIR=$DepsPrefix","-DOPENSSL_USE_STATIC_LIBS=TRUE","-DZLIB_ROOT=$DepsPrefix","-DZLIB_LIBRARY=$ZlibStatic",
    "-DCURL_USE_LIBPSL=OFF","-DCURL_BROTLI=OFF","-DCURL_ZSTD=OFF","-DUSE_LIBIDN2=OFF","-DUSE_NGHTTP2=OFF","-DUSE_NGTCP2=OFF","-DUSE_QUICHE=OFF","-DCURL_USE_LIBSSH2=OFF","-DCURL_USE_GSSAPI=OFF",
    "-DCURL_DISABLE_LDAP=ON","-DCURL_DISABLE_LDAPS=ON","-DCURL_DISABLE_FTP=ON","-DCURL_DISABLE_FILE=ON","-DCURL_DISABLE_TELNET=ON","-DCURL_DISABLE_TFTP=ON","-DCURL_DISABLE_DICT=ON","-DCURL_DISABLE_GOPHER=ON","-DCURL_DISABLE_IMAP=ON","-DCURL_DISABLE_POP3=ON","-DCURL_DISABLE_RTSP=ON","-DCURL_DISABLE_SMB=ON","-DCURL_DISABLE_SMTP=ON","-DCURL_DISABLE_MQTT=ON","-DCURL_DISABLE_WEBSOCKETS=ON"
  )
}
if (-not (Test-Path $CurlStatic)) { throw "Static libcurl missing" }

@("openssl=$OpenSSLVersion sha256=$OsslExpected","zlib=$ZlibVersion sha256=$ZlibSha","libpng=$PngVersion sha256=$PngSha","curl=$CurlVersion sha256=$CurlSha","os=windows","arch=x86_64") | Set-Content -Encoding ascii (Join-Path $DepsPrefix "qrx-deps.lock")

if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
$cmakeArgs=@("-S",$Core,"-B",$BuildDir,"-A","x64","-DCMAKE_BUILD_TYPE=Release","-DQRX_REQUIRE_PQC=ON","-DQRX_REQUIRE_BUNDLED_DEPS=ON","-DQRX_DEPS_PREFIX=$DepsPrefix","-DOPENSSL_ROOT_DIR=$DepsPrefix","-DOPENSSL_USE_STATIC_LIBS=TRUE","-DOPENSSL_CRYPTO_LIBRARY=$Crypto","-DZLIB_ROOT=$DepsPrefix","-DZLIB_LIBRARY=$ZlibStatic","-DZLIB_INCLUDE_DIR=$DepsPrefix\include","-DPNG_PNG_INCLUDE_DIR=$DepsPrefix\include","-DPNG_LIBRARY=$PngStatic","-DCURL_ROOT=$DepsPrefix","-DCURL_USE_STATIC_LIBS=TRUE","-DCURL_LIBRARY=$CurlStatic","-DCURL_INCLUDE_DIR=$DepsPrefix\include")
& cmake @cmakeArgs; if ($LASTEXITCODE -ne 0) { throw "QRX CMake configure failed" }
& cmake --build $BuildDir --config Release --parallel $Jobs; if ($LASTEXITCODE -ne 0) { throw "QRX build failed" }
$expected=@("qrx.exe","qrx-cli.exe","qrxd.exe","qrx-upscaler.exe","qrxdb_verify.exe","qrxdb_salvage.exe","qrxdb_compact.exe","qrxdb_snapshot.exe")
foreach($name in $expected){$p=Join-Path (Join-Path $BuildDir "Release") $name;if(-not(Test-Path $p)){throw "Expected artifact missing: $p"}}
Write-Host "Hermetic Windows x64 QRX build complete: $BuildDir\Release"
Write-Host "Dependency prefix: $DepsPrefix"
