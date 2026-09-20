@echo off
setlocal
cd /d "%~dp0.."
echo QRX 0.0.9.82 Windows x64 policy bootstrap
echo.
echo Windows may block unsigned local PowerShell scripts under the current execution policy.
echo This launcher can start ONE child PowerShell process with -ExecutionPolicy Bypass.
echo It does NOT change the machine or user execution-policy setting.
choice /C YN /N /M "Allow this one QRX build process to bypass PowerShell execution policy? [Y/N] "
if errorlevel 2 (
  echo Build cancelled. No execution-policy change was made.
  exit /b 2
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-all-targets.ps1" -Target windows-x64 %*
set RC=%ERRORLEVEL%
exit /b %RC%
