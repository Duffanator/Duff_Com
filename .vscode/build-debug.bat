@echo off
setlocal

set DOS_DIR=%USERPROFILE%\.dos
if exist "%~dp0..\.dos\djgpp\bin\i586-pc-msdosdjgpp-gcc.exe" (
    set DOS_DIR=%~dp0..\.dos
)

if not exist "%~dp0..\build" mkdir "%~dp0..\build"
if not exist "%~dp0..\build\assets" mkdir "%~dp0..\build\assets"

"%DOS_DIR%\djgpp\bin\i586-pc-msdosdjgpp-gcc.exe" -g -Wall -std=gnu99 ^
    -I"%DOS_DIR%\djgpp\i586-pc-msdosdjgpp\sys-include" ^
    "%~dp0..\src\main.c" "%~dp0..\src\vga.c" "%~dp0..\src\input.c" "%~dp0..\src\sound.c" "%~dp0..\src\music.c" "%~dp0..\src\settings.c" ^
    "%~dp0..\src\bmp_loader.c" "%~dp0..\src\gfx_assets.c" "%~dp0..\src\items.c" "%~dp0..\src\operative.c" "%~dp0..\src\pathfind.c" "%~dp0..\src\procgen.c" "%~dp0..\src\isomap.c" "%~dp0..\src\hud.c" "%~dp0..\src\base_ui.c" "%~dp0..\src\campaign.c" ^
    -lm -o "%~dp0..\build\main.exe"

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

copy /Y "%~dp0..\ROSTER.CFG" "%~dp0..\build\ROSTER.CFG" >nul
if not exist "%~dp0..\build\SETTINGS.CFG" copy /Y "%~dp0..\SETTINGS.CFG" "%~dp0..\build\SETTINGS.CFG" >nul
copy /Y "%~dp0..\assets\*.*" "%~dp0..\build\assets\" >nul
