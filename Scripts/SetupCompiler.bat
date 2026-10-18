@echo off
rem Installs and pins the MSVC compiler UE 5.5 needs (see SetupCompiler.ps1). Run once per PC, then BuildEditor.bat.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0SetupCompiler.ps1" %*
if errorlevel 1 (
    echo.
    echo SetupCompiler failed; see the message above.
)
pause
