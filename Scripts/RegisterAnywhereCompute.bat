@echo off
rem One-time (per machine/IP): registers this PC as a compute in the GameLift Anywhere fleet.
rem Prints the SDK WebSocket URL; put it in Config.local.bat as GAMELIFT_WEBSOCKET_URL.
rem Requires the AWS CLI and the GAMELIFT_* settings in Config.local.bat.
rem Usage: RegisterAnywhereCompute.bat [IpAddress]   (default 127.0.0.1: only this PC can connect)
setlocal
call "%~dp0Config.bat" || exit /b 1

if not defined GAMELIFT_FLEET_ID (
    echo ERROR: Set GAMELIFT_FLEET_ID, GAMELIFT_LOCATION and GAMELIFT_COMPUTE_NAME in Scripts\Config.local.bat.
    exit /b 1
)

set "IP=%~1"
if "%IP%"=="" set "IP=127.0.0.1"

aws gamelift register-compute ^
    --region %GAMELIFT_REGION% ^
    --fleet-id %GAMELIFT_FLEET_ID% ^
    --compute-name %GAMELIFT_COMPUTE_NAME% ^
    --ip-address %IP% ^
    --location %GAMELIFT_LOCATION% ^
    --query "Compute.GameLiftServiceSdkEndpoint" --output text
exit /b %ERRORLEVEL%
