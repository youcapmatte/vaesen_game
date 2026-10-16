@echo off
rem Starts the packaged Win64 dedicated server as a GameLift Anywhere process on this PC.
rem The server calls ProcessReady and waits; Vaesen.Api (Sessions:Provider=GameLift, FleetId + Location set)
rem then creates the world's game session on it. Run once per world/port you want hosted.
rem Usage: RunAnywhereServer.bat [Port] [Development|Shipping]
setlocal
call "%~dp0Config.bat" || exit /b 1

set "PORT=%~1"
if "%PORT%"=="" set "PORT=7777"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

if not defined GAMELIFT_WEBSOCKET_URL (
    echo ERROR: Set GAMELIFT_WEBSOCKET_URL in Scripts\Config.local.bat ^(run RegisterAnywhereCompute.bat first^).
    exit /b 1
)

set "EXE=%BUILDS_DIR%\Server\Win64\%CONFIG%\WindowsServer\%PROJECT_NAME%Server.exe"
if /i not "%CONFIG%"=="Development" set "EXE=%BUILDS_DIR%\Server\Win64\%CONFIG%\WindowsServer\%PROJECT_NAME%Server-Win64-%CONFIG%.exe"
if not exist "%EXE%" (
    echo ERROR: "%EXE%" not found. Run PackageServer.bat Win64 %CONFIG% first.
    exit /b 1
)

rem Auth tokens are short-lived, so fetch a fresh one on every start.
set "AUTH_TOKEN="
for /f "usebackq delims=" %%T in (`aws gamelift get-compute-auth-token --region %GAMELIFT_REGION% --fleet-id %GAMELIFT_FLEET_ID% --compute-name %GAMELIFT_COMPUTE_NAME% --query AuthToken --output text`) do set "AUTH_TOKEN=%%T"
if not defined AUTH_TOKEN (
    echo ERROR: Could not get a compute auth token. Check AWS credentials and that the compute is registered.
    exit /b 1
)

start "Vaesen Anywhere Server :%PORT%" "%EXE%" -port=%PORT% -log ^
    -GameLiftAnywhere ^
    -GameLiftWebSocketUrl=%GAMELIFT_WEBSOCKET_URL% ^
    -GameLiftFleetId=%GAMELIFT_FLEET_ID% ^
    -GameLiftHostId=%GAMELIFT_COMPUTE_NAME% ^
    -GameLiftAuthToken=%AUTH_TOKEN% ^
    -GameLiftRegion=%GAMELIFT_REGION%
