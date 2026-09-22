@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"C:\Program Files\CMake\bin\cmake.exe" -S Tools/Updater -B Native/build-update-helper -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
"C:\Program Files\CMake\bin\cmake.exe" --build Native/build-update-helper
if errorlevel 1 exit /b 1
copy /y Native\build-update-helper\Idas3UpdateInstaller.exe Assets\Resources\UpdateInstaller.exe.bytes >nul
