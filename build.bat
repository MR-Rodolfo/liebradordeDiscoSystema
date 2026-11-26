@echo off
REM  ____  _     _      ____
REM |  _ \(_)___| | __ / ___|_      _____  ___ _ __
REM | | | | / __| |/ / \___ \ \ /\ / / _ \/ _ \ '_ \
REM | |_| | \__ \   <   ___) \ V  V /  __/  __/ |_) |
REM |____/|_|___/_|\_\ |____/ \_/\_/ \___|\___| .__/
REM                                            |_|
REM  DiskSweep v2.0.0 - compilacion rapida

where g++ >nul 2>nul
if %errorlevel%==0 (
    g++ DiskSweep.cpp -o DiskSweep.exe -static
    goto done
)
where cl >nul 2>nul
if %errorlevel%==0 (
    cl /EHsc /O2 DiskSweep.cpp
    goto done
)
echo No se encontro g++ ni cl. Instala MinGW o Visual Studio Build Tools.
exit /b 1

:done
echo.
echo Listo: DiskSweep.exe
