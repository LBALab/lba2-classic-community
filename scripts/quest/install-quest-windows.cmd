@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-quest-windows.ps1" %*
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" (
    echo.
    pause
)

exit /b %RC%
