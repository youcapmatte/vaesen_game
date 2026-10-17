@echo off
rem Starts the packaged Win64 dedicated server in Builds\CurrentGameliftAnywhere as GameLift Anywhere processes on this PC.
rem To update it: PackageServer.bat Win64 Development, then move Builds\Server\Win64\Development's contents there.
rem Each process calls ProcessReady and waits; Vaesen.Api (Sessions:Provider=GameLift, FleetId + Location set) then
rem creates game sessions on them. A process hosts one game session (one map instance) at a time, so start one per
rem world you want running at once: e.g. 3 = a guild hub, a dungeon and a spare.
rem Usage: RunAnywhereServer.bat [Count] [FirstPort]
rem   Count      number of server processes (default 1)
rem   FirstPort  port of the first one (default 7777); the others use the next ports: 7778, 7779, ...
setlocal EnableDelayedExpansion
call "%~dp0Config.bat" || exit /b 1

set "COUNT=%~1"
if "%COUNT%"=="" set "COUNT=1"
set "FIRST_PORT=%~2"
if "%FIRST_PORT%"=="" set "FIRST_PORT=7777"
set /a COUNT=COUNT
if %COUNT% LSS 1 (
    echo ERROR: Count must be 1 or more.
    exit /b 1
)

if not defined GAMELIFT_WEBSOCKET_URL (
    echo ERROR: Set GAMELIFT_WEBSOCKET_URL in Scripts\Config.local.bat ^(run RegisterAnywhereCompute.bat first^).
    exit /b 1
)

set "EXE=%BUILDS_DIR%\CurrentGameliftAnywhere\%PROJECT_NAME%Server.exe"
if not exist "%EXE%" (
    echo ERROR: "%EXE%" not found. Run PackageServer.bat Win64 Development and move the result to Builds\CurrentGameliftAnywhere.
    exit /b 1
)

rem Auth tokens are short-lived, so fetch a fresh one on every start. All processes on this compute share it.
set "AUTH_TOKEN="
for /f "usebackq delims=" %%T in (`aws gamelift get-compute-auth-token --region %GAMELIFT_REGION% --fleet-id %GAMELIFT_FLEET_ID% --compute-name %GAMELIFT_COMPUTE_NAME% --query AuthToken --output text`) do set "AUTH_TOKEN=%%T"
if not defined AUTH_TOKEN (
    echo ERROR: Could not get a compute auth token. Check AWS credentials and that the compute is registered.
    exit /b 1
)

for /L %%I in (1,1,%COUNT%) do (
    set /a PORT=FIRST_PORT+%%I-1
    echo Starting server %%I of %COUNT% on port !PORT!
    start "Vaesen Anywhere Server :!PORT!" "%EXE%" -port=!PORT! -log ^
        -GameLiftAnywhere ^
        -GameLiftWebSocketUrl=%GAMELIFT_WEBSOCKET_URL% ^
        -GameLiftFleetId=%GAMELIFT_FLEET_ID% ^
        -GameLiftHostId=%GAMELIFT_COMPUTE_NAME% ^
        -GameLiftAuthToken=!AUTH_TOKEN! ^
        -GameLiftRegion=%GAMELIFT_REGION%
)
exit /b 0
