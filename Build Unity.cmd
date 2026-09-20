@echo off
setlocal
call "%~dp0Build Native.cmd"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Unity-Project.ps1" -Action Build
exit /b %errorlevel%
