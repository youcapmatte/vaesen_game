@echo off
rem Regenerates VaesenGame.sln against the source engine.
setlocal
call "%~dp0Config.bat" || exit /b 1

call "%UE_ROOT%\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="%UPROJECT%" -game -engine
exit /b %ERRORLEVEL%
