param(
  [ValidateSet('host','linux-x64','linux-arm64','macos-x64','macos-arm64','windows-x64')]
  [string]$Target='host',
  [switch]$Plan
)
$ErrorActionPreference='Stop'
$script = Join-Path $PSScriptRoot 'build-all-targets.sh'
$bash = Get-Command bash -ErrorAction SilentlyContinue
if (-not $bash) { throw 'bash is required. On Windows use Git for Windows/MSYS2 or the GitHub Actions native Windows runner.' }
$args = @($script, '--target', $Target)
if ($Plan) { $args += '--plan' }
& $bash.Source @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
