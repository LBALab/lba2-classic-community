@echo off
setlocal

rem The PowerShell installer prints the result and waits for a key before
rem exiting, so this launcher must not pause a second time.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-quest-windows.ps1" %*
exit /b %ERRORLEVEL%
