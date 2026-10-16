@echo off
rem Starts the packaged Win64 dedicated server locally with a log window.
rem Usage: RunLocalServer.bat [Development|Shipping] [Port]
setlocal
call "%~dp0Config.bat" || exit /b 1

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"
set "PORT=%~2"
if "%PORT%"=="" set "PORT=7777"

set "EXE=%BUILDS_DIR%\Server\Win64\%CONFIG%\WindowsServer\%PROJECT_NAME%Server.exe"
if /i not "%CONFIG%"=="Development" set "EXE=%BUILDS_DIR%\Server\Win64\%CONFIG%\WindowsServer\%PROJECT_NAME%Server-Win64-%CONFIG%.exe"
if not exist "%EXE%" (
    echo ERROR: "%EXE%" not found. Run PackageServer.bat Win64 %CONFIG% first.
    exit /b 1
)

start "Vaesen Server :%PORT%" "%EXE%" -log -port=%PORT%
