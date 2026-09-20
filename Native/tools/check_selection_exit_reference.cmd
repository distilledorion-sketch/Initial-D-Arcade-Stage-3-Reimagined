@echo off
setlocal
if "%~1"=="" (
  echo Usage: check_selection_exit_reference.cmd path-to-idas3_main_0C020000.bin
  exit /b 2
)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist "work\selection_exit_reference" mkdir "work\selection_exit_reference"
cl /nologo /O2 /std:c++20 /EHsc /MT "%~dp0check_selection_exit_reference.cpp" /Fowork\selection_exit_reference\ /Fework\selection_exit_reference\check.exe
if errorlevel 1 exit /b %errorlevel%
work\selection_exit_reference\check.exe "%~1"
exit /b %errorlevel%
