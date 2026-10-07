@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-InitialD.ps1"
if errorlevel 1 echo Installation did not complete. Read the error above; existing game files were not replaced.
pause
