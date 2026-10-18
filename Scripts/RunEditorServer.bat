@echo off
rem Starts a dedicated server from the editor build (uncooked content, no packaging needed) WITHOUT GameLift.
rem The quickest way to test world join with a Standalone client from the editor: run Vaesen.Api with
rem Sessions:Provider=Local, start this, then press Play on a character.
rem Usage: RunEditorServer.bat [Port] [Map]
rem   RunEditorServer.bat          (port 7777, L_Gyllencreutz: every guild hub in the API's Local mode)
setlocal
call "%~dp0Config.bat" || exit /b 1

set "PORT=%~1"
if "%PORT%"=="" set "PORT=7777"
set "MAP=%~2"
if "%MAP%"=="" set "MAP=/Game/Vaesen/Maps/L_Gyllencreutz"

if not exist "%UE_EDITOR%" (
    echo ERROR: "%UE_EDITOR%" not found. Build the editor first ^(BuildEditor.bat^).
    exit /b 1
)

start "Vaesen Editor Server :%PORT%" "%UE_EDITOR%" "%UPROJECT%" %MAP% -server -port=%PORT% -NoGameLift -log
exit /b 0
