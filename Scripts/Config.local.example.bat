@echo off
rem Copy to Config.local.bat (git-ignored) and fill in for this machine.

rem set "UE_ROOT=F:\UE_SOURCE\UE5_5\UnrealEngine"

rem --- GameLift Anywhere (RegisterAnywhereCompute.bat / RunAnywhereServer.bat) ---
rem AWS CLI profile with GameLift permissions.
set "AWS_PROFILE=vaesen"
set "GAMELIFT_REGION=eu-north-1"
set "GAMELIFT_FLEET_ID=fleet-00000000-0000-0000-0000-000000000000"
rem Custom location attached to the Anywhere fleet.
set "GAMELIFT_LOCATION=custom-vaesen-dev"
rem Name this PC is registered under in the fleet.
set "GAMELIFT_COMPUTE_NAME=%COMPUTERNAME%"
rem Printed by RegisterAnywhereCompute.bat.
set "GAMELIFT_WEBSOCKET_URL="
