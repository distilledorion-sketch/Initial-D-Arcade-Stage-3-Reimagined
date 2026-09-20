@echo off
setlocal
cd /d "%~dp0.."
set "IDAS3_VS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "IDAS3_VS=%%i"
if not defined IDAS3_VS exit /b 1
call "%IDAS3_VS%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
cmake --build Native/build-unity --target unity_scene_smoke unity_shared_renderer_tests
exit /b %errorlevel%
