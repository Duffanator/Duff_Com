@echo off
setlocal

set DOS_DIR=%USERPROFILE%\.dos
if exist "%~dp0.dos\djgpp\bin\i586-pc-msdosdjgpp-gcc.exe" (
    set DOS_DIR=%~dp0.dos
)

if not exist "%DOS_DIR%\djgpp\bin\i586-pc-msdosdjgpp-gcc.exe" (
    echo [!] DJGPP compiler not found at "%DOS_DIR%\djgpp".
    echo [!] In VS Code, press Ctrl+Shift+P and run:  DOS: Install tools
    pause
    exit /b 1
)

if not exist dist mkdir dist
if not exist dist\assets mkdir dist\assets
if not exist build mkdir build
if not exist build\assets mkdir build\assets

echo [1/2] Compiling DUFF-COM (32-bit Protected Mode MS-DOS)...
"%DOS_DIR%\djgpp\bin\i586-pc-msdosdjgpp-gcc.exe" -O2 -Wall -std=gnu99 -DNDEBUG ^
    -I"%DOS_DIR%\djgpp\i586-pc-msdosdjgpp\sys-include" ^
    src/main.c src/vga.c src/input.c src/sound.c src/music.c src/settings.c src/bmp_loader.c src/gfx_assets.c src/items.c src/operative.c src/pathfind.c src/procgen.c src/isomap.c src/hud.c src/base_ui.c src/campaign.c ^
    -lm -o dist/DUFFCOM.EXE

if %ERRORLEVEL% NEQ 0 (
    echo [!] Compilation failed!
    pause
    exit /b 1
)

copy /Y dist\DUFFCOM.EXE build\main.exe >nul
copy /Y ROSTER.CFG dist\ROSTER.CFG >nul
if not exist dist\SETTINGS.CFG copy /Y SETTINGS.CFG dist\SETTINGS.CFG >nul
copy /Y assets\*.* dist\assets\ >nul
copy /Y assets\*.* build\assets\ >nul

echo [2/2] Launching DUFFCOM.EXE in DOSBox-X...
"%DOS_DIR%\dosbox-x\dosbox-x.exe" -conf "%~dp0dosbox-duffcom.conf" -fastlaunch -exit "%~dp0dist\DUFFCOM.EXE"
