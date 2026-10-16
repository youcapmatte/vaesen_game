@echo off
rem Builds, cooks and packages the client-only game into Builds\Client\Win64\<Config>.
rem Usage: PackageClient.bat [Development|Shipping]
setlocal
call "%~dp0Config.bat" || exit /b 1

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"
set "OUT_DIR=%BUILDS_DIR%\Client\Win64\%CONFIG%"

call "%UAT%" BuildCookRun ^
    -project="%UPROJECT%" ^
    -noP4 -utf8output -unattended ^
    -client ^
    -platform=Win64 -clientconfig=%CONFIG% ^
    -build -cook -stage -pak -package -archive ^
    -archivedirectory="%OUT_DIR%"
if errorlevel 1 exit /b 1

echo.
echo Client packaged to "%OUT_DIR%"
exit /b 0
