from pathlib import Path
r=Path(__file__).resolve().parents[1]
a=(r/'scripts/build-all-targets.ps1').read_text()
b=(r/'qrx-core/scripts/build-windows-x64-static.ps1').read_text()
assert "'git'" in a and "Install-WingetPackage 'Git.Git'" in a
assert r"C:\Program Files\Git\cmd" in a
assert 'foreach ($c in @("cmake","perl","tar","git"))' in b
assert '$TarExe = Join-Path $env:SystemRoot "System32\\tar.exe"' in b
assert '& $TarExe -xf $Archive --strip-components=1 -C $Destination' in b
assert '$CMakeGenerator="Visual Studio 17 2022"' in b
assert '-G $CMakeGenerator -A x64' in b
assert '"-G",$CMakeGenerator,"-A","x64"' in b
assert '& cmake -S $Source -B $Build -A x64' not in b
print('QRX 0.0.9.93 Windows CMake/Git bootstrap audit: PASS')
