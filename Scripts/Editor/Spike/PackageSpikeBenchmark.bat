@echo off
rem Phase 0 spike (P0-10): packages a client with only the benchmark levels (L_SpikeBenchmark and its empty baseline) into
rem Builds\Spike\Win64\Development. Run the measurement with RunSpikeBenchmark.bat.
setlocal
call "%~dp0..\..\Config.bat" || exit /b 1

set "OUT_DIR=%BUILDS_DIR%\Spike\Win64\Development"

call "%UAT%" BuildCookRun ^
    -project="%UPROJECT%" ^
    -noP4 -utf8output -unattended ^
    -client ^
    -platform=Win64 -clientconfig=Development ^
    -map=/Game/Spike/Maps/L_SpikeBenchmark+/Game/Spike/Maps/L_SpikeBenchmarkEmpty ^
    -build -cook -stage -pak -package -archive ^
    -archivedirectory="%OUT_DIR%"
if errorlevel 1 exit /b 1

echo.
echo Spike benchmark packaged to "%OUT_DIR%"
exit /b 0
