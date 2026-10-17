@echo off
rem Starts the packaged Win64 client and connects it to a server.
rem Usage: RunLocalClient.bat [Address:Port] [Development|Shipping]
setlocal
call "%~dp0Config.bat" || exit /b 1

set "ADDRESS=%~1"
if "%ADDRESS%"=="" set "ADDRESS=127.0.0.1:7777"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

set "EXE=%BUILDS_DIR%\Client\Win64\%CONFIG%\%PROJECT_NAME%Client.exe"
if not exist "%EXE%" (
    echo ERROR: "%EXE%" not found. Run PackageClient.bat %CONFIG% first.
    exit /b 1
)

start "Vaesen Client" "%EXE%" %ADDRESS% -windowed -resx=1280 -resy=720 -log
