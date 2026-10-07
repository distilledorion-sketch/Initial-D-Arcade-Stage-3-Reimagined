@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Unity-Project.ps1" -Action BuildAndroid
exit /b %errorlevel%
