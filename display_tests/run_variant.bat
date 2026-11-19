@echo off
rem Usage: display_tests\run_variant.bat 1_sharp   (or 2_advinterp, 3_xbrz, 4_crt_scan)
rem Launches the already-built dist\DUFFCOM.EXE in DOSBox-X with that display config.
setlocal
if "%~1"=="" (
    echo Usage: %~nx0 ^<variant^>   e.g. 1_sharp, 2_advinterp, 3_xbrz, 4_crt_scan
    exit /b 1
)
set DOS_DIR=%USERPROFILE%\.dos
if exist "%~dp0..\.dos\dosbox-x\dosbox-x.exe" set DOS_DIR=%~dp0..\.dos
"%DOS_DIR%\dosbox-x\dosbox-x.exe" -conf "%~dp0dosbox-%~1.conf" -fastlaunch -exit "%~dp0..\dist\DUFFCOM.EXE"
