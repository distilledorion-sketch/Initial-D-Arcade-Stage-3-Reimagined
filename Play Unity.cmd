@echo off
if not exist "%~dp0Builds\Current\InitialDUnity.exe" (
  echo Run Build Unity.cmd with the installed Unity Editor first.
  pause
  exit /b 2
)
start "Initial D Unity" /D "%~dp0Builds\Current" "%~dp0Builds\Current\InitialDUnity.exe" -force-d3d11
