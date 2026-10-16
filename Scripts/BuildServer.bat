@echo off
rem Compiles the dedicated server binary only (no cook/package). Quick check that server code builds.
rem Usage: BuildServer.bat [Win64|Linux] [Development|Shipping|DebugGame]
setlocal
call "%~dp0Config.bat" || exit /b 1

set "PLATFORM=%~1"
if "%PLATFORM%"=="" set "PLATFORM=Win64"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UBT%" %PROJECT_NAME%Server %PLATFORM% %CONFIG% -Project="%UPROJECT%" -WaitMutex
exit /b %ERRORLEVEL%
