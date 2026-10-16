@echo off
rem Builds, cooks and packages the dedicated server into Builds\Server\<Platform>\<Config>.
rem Usage: PackageServer.bat [Win64|Linux] [Development|Shipping]
rem Linux requires the UE 5.5 cross-compile toolchain (LINUX_MULTIARCH_ROOT).
setlocal
call "%~dp0Config.bat" || exit /b 1

set "PLATFORM=%~1"
if "%PLATFORM%"=="" set "PLATFORM=Win64"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"
set "OUT_DIR=%BUILDS_DIR%\Server\%PLATFORM%\%CONFIG%"

call "%UAT%" BuildCookRun ^
    -project="%UPROJECT%" ^
    -noP4 -utf8output -unattended ^
    -server -noclient ^
    -serverplatform=%PLATFORM% -serverconfig=%CONFIG% ^
    -build -cook -stage -pak -package -archive ^
    -archivedirectory="%OUT_DIR%"
if errorlevel 1 exit /b 1

echo.
echo Server packaged to "%OUT_DIR%"
exit /b 0
