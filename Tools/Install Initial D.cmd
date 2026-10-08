@echo off
setlocal DisableDelayedExpansion
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-InitialD.ps1"
set "InitialDSetupExit=%errorlevel%"
if not "%InitialDSetupExit%"=="0" echo Installation did not complete. Read the error above; existing game files were not replaced.
pause
exit /b %InitialDSetupExit%
