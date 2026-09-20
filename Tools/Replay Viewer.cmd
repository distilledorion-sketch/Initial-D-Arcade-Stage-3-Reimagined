@echo off
if "%~1"=="" (
  start "Initial D Replay Viewer" "%~dp0InitialDUnity.exe" -idas3-replay-viewer
) else (
  start "Initial D Replay Viewer" "%~dp0InitialDUnity.exe" -idas3-replay-viewer "%~f1"
)
