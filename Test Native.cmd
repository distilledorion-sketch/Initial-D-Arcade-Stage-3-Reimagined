@echo off
setlocal
cd /d "%~dp0"
set "IDAS3_VS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "IDAS3_VS=%%i"
if not defined IDAS3_VS exit /b 1
call "%IDAS3_VS%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
cmake -S Native -B Native/build-unity -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
cmake --build Native/build-unity --target Idas3Unity unity_shared_renderer_tests unity_bridge_smoke
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir Native/build-unity -R "^unity_shared_renderer$" --output-on-failure
if errorlevel 1 exit /b %errorlevel%
for /f %%i in ('powershell.exe -NoProfile -Command "[Guid]::NewGuid().ToString()"') do set "IDAS3_SMOKE_ID=%%i"
Native\bin\unity_bridge_smoke.exe "%~dp0Assets\Plugins\x86_64\Idas3Unity.dll" "%~dp0Native" "%~dp0Staging\bridge-smoke-%IDAS3_SMOKE_ID%"
exit /b %errorlevel%
