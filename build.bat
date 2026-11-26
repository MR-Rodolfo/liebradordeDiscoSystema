@echo off
REM  ____  _     _      ____
REM |  _ \(_)___| | __ / ___|_      _____  ___ _ __
REM | | | | / __| |/ / \___ \ \ /\ / / _ \/ _ \ '_ \
REM | |_| | \__ \   <   ___) \ V  V /  __/  __/ |_) |
REM |____/|_|___/_|\_\ |____/ \_/\_/ \___|\___| .__/
REM                                            |_|
REM  DiskSweep v2.0.0 - compilacion rapida (con icono)

REM Si no hay assets\logo.ico, se genera desde assets\logo.ico.b64 (texto)
if not exist assets\logo.ico (
    if exist assets\logo.ico.b64 (
        certutil -f -decode assets\logo.ico.b64 assets\logo.ico >nul
    ) else (
        echo Aviso: falta assets\logo.ico, se compilara sin icono.
    )
)

where g++ >nul 2>nul
if %errorlevel%==0 (
    windres DiskSweep.rc -O coff -o DiskSweep_res.o
    if exist DiskSweep_res.o (
        g++ DiskSweep.cpp DiskSweep_res.o -o DiskSweep.exe -static
    ) else (
        echo Aviso: no se pudo crear el icono, compilando sin el.
        g++ DiskSweep.cpp -o DiskSweep.exe -static
    )
    goto done
)
where cl >nul 2>nul
if %errorlevel%==0 (
    rc /nologo DiskSweep.rc
    if exist DiskSweep.res (
        cl /nologo /EHsc /O2 DiskSweep.cpp DiskSweep.res
    ) else (
        echo Aviso: no se pudo crear el icono, compilando sin el.
        cl /nologo /EHsc /O2 DiskSweep.cpp
    )
    goto done
)
echo No se encontro g++ ni cl. Instala MinGW o Visual Studio Build Tools.
exit /b 1

:done
echo.
echo Listo: DiskSweep.exe
