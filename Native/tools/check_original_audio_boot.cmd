@echo off
setlocal
if "%~2"=="" (
 echo Usage: check_original_audio_boot.cmd source-image HOSTFS-directory
 exit /b 2
)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist "work\audio_boot_reference" mkdir "work\audio_boot_reference"
cl /nologo /O2 /std:c++20 /EHsc /MT "%~dp0check_original_audio_boot.cpp" /Fowork\audio_boot_reference\ /Fework\audio_boot_reference\check.exe
if errorlevel 1 exit /b %errorlevel%
work\audio_boot_reference\check.exe "%~1" "%~2"
exit /b %errorlevel%
