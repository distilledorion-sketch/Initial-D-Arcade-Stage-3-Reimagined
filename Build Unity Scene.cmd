@echo off
setlocal
call "%~dp0Build Unity.cmd"
exit /b %errorlevel%
