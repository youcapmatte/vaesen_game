@echo off
rem Starts the packaged Win64 dedicated server locally WITHOUT GameLift (-NoGameLift).
rem Pair with Vaesen.Api in Development (Sessions:Provider=Local): each world is one server on its LocalPort.
rem Usage: RunLocalServer.bat [Port] [Map] [Development|Shipping]
rem   RunLocalServer.bat 7777   (world "gyllencreutz": every guild hub, in the API config)
rem   RunLocalServer.bat 7778 /Engine/Maps/Templates/OpenWorld   (world "town")
setlocal
call "%~dp0Config.bat" || exit /b 1

set "PORT=%~1"
if "%PORT%"=="" set "PORT=7777"
set "MAP=%~2"
if "%MAP%"=="" set "MAP=/Game/Vaesen/Maps/L_Gyllencreutz"
set "CONFIG=%~3"
if "%CONFIG%"=="" set "CONFIG=Development"

call :FindServerExe || exit /b 1
start "Vaesen Server :%PORT%" "%EXE%" %MAP% -port=%PORT% -NoGameLift -log
exit /b 0

:FindServerExe
set "EXE=%BUILDS_DIR%\Server\Win64\%CONFIG%\%PROJECT_NAME%Server.exe"
if not exist "%EXE%" (
    echo ERROR: "%EXE%" not found. Run PackageServer.bat Win64 %CONFIG% first.
    exit /b 1
)
exit /b 0
