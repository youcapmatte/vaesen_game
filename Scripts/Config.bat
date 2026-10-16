@echo off
rem Shared paths for all Vaesen scripts. Called by the other scripts; not meant to be run directly.
rem Override UE_ROOT by setting it as an environment variable before running a script.

if not defined UE_ROOT set "UE_ROOT=F:\UE_SOURCE\UE5_5\UnrealEngine"

set "VAESEN_ROOT=%~dp0.."
for %%I in ("%VAESEN_ROOT%") do set "VAESEN_ROOT=%%~fI"

set "PROJECT_NAME=VaesenGame"
set "PROJECT_DIR=%VAESEN_ROOT%\VaesenGame"
set "UPROJECT=%PROJECT_DIR%\%PROJECT_NAME%.uproject"
set "BUILDS_DIR=%VAESEN_ROOT%\Builds"

set "UBT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
set "UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"
set "UE_EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%UAT%" (
    echo ERROR: Unreal Engine not found at "%UE_ROOT%". Set UE_ROOT to your source engine folder.
    exit /b 1
)
exit /b 0
