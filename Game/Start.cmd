@echo off
setlocal
pushd "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\Engine\BuildSupport\Start-Launcher.ps1"
set "buildResult=%errorlevel%"
if not "%buildResult%"=="0" pause
popd
exit /b %buildResult%
