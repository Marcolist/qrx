from pathlib import Path
p=Path(__file__).resolve().parents[1]/'qrx-core/scripts/build-windows-x64-static.ps1'
s=p.read_text()
assert '$PngTag="v$PngVersion"' in s
assert '--branch $PngTag --single-branch' in s
assert 'rev-parse HEAD' in s
assert 'describe --tags --exact-match HEAD' in s
assert '3061454d980de7d53608f594194cfac722721d2a' in s
assert 'refs/tags/v$PngVersion:refs/tags/v$PngVersion' not in s
print('QRX 0.0.9.92 Windows libpng tag resolution audit: PASS')
