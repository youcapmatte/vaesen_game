@echo off
rem Compiles the editor target (VaesenGameEditor Win64 Development).
setlocal
call "%~dp0Config.bat" || exit /b 1

call "%UBT%" %PROJECT_NAME%Editor Win64 Development -Project="%UPROJECT%" -WaitMutex
exit /b %ERRORLEVEL%
