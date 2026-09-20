@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Unity-Project.ps1" -Action Open
if errorlevel 1 pause
