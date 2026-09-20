@echo off
setlocal
cd /d "%~dp0"
set "IDAS3_VS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "IDAS3_VS=%%i"
if not defined IDAS3_VS (
  echo MSVC C++ build tools are required.
  exit /b 1
)
call "%IDAS3_VS%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
cmake -S Native -B Native/build-unity-d -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
cmake --build Native/build-unity-d --target Idas3Unity
if errorlevel 1 exit /b %errorlevel%
echo Native Unity plugin built successfully.
