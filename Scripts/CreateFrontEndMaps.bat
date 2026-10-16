@echo off
rem Creates L_Login and L_CharacterSelect (with their GameMode overrides) by running Editor\CreateFrontEndMaps.py headless.
rem Build the editor first (BuildEditor.bat). Close the editor before running.
setlocal
call "%~dp0Config.bat" || exit /b 1

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%UPROJECT%" -run=pythonscript -script="%~dp0Editor\CreateFrontEndMaps.py" -unattended -nop4 -nosplash -stdout
exit /b %ERRORLEVEL%
