@echo off
setlocal
pushd "%~dp0"
set "ROM=%~1"
if not defined ROM for /f "delims=" %%R in ('dir /b /a-d "rom-game\*.nes" 2^>nul') do if not defined ROM set "ROM=rom-game\%%R"
if defined ROM if exist "%ROM%" goto rom_ready
if defined ROM if exist "rom-game\%ROM%" set "ROM=rom-game\%ROM%"
if defined ROM if exist "%ROM%" goto rom_ready
if not defined ROM goto missing_rom
goto missing_rom
:rom_ready
if /i "%~2"=="--recompiled" goto recompiled
if not exist "build\Release\nes_window.exe" goto missing_runtime
echo Starting interpreter with "%ROM%"...
call "build\Release\nes_window.exe" "%ROM%"
goto finished

:recompiled
if not exist "build\Release\game.exe" goto missing_game
echo Starting generated build with "%ROM%"...
call "build\Release\game.exe" "%ROM%" --recompiled

:finished
echo.
echo NES runtime exited with code %ERRORLEVEL%.
pause
popd
exit /b 0

:missing_game
echo Missing build\Release\game.exe
echo Build it with: cmake --build build --config Release --target game
pause
exit /b 1

:missing_runtime
echo Missing build\Release\nes_window.exe
echo Build it with: cmake --build build --config Release
pause
exit /b 1

:missing_rom
echo No valid ROM found. Pass a ROM path, for example:
echo run_game.bat "rom-game\Super Mario Bros. 3 (USA) (Rev 1).nes"
pause
exit /b 1
