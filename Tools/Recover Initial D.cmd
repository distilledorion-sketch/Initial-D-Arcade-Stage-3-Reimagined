@echo off
setlocal DisableDelayedExpansion
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Recover-InitialD.ps1"
set "recoveryExit=%errorlevel%"
pause
exit /b %recoveryExit%
