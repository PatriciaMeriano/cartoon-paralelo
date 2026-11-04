@echo off
title Generador de Cartoon - Menu Central
color 0B
setlocal enabledelayedexpansion

:: ============ CONFIGURACIÓN DE CARPETAS ============
set "CARPETA_SEQ=C:\2026\CA\FinalMio\secuencial"
set "CARPETA_MPI=C:\2026\CA\FinalMio\mpi"
set "CARPETA_OMP=C:\2026\CA\FinalMio\openmp"
set "CARPETA_HIB=C:\2026\CA\FinalMio\hibrida"
:: ===================================================

:menu
cls
echo ==========================================
echo        GENERADOR DE CARTOON
echo ==========================================
echo.
echo  1. Ejecutar SECUENCIAL
echo     Carpeta: %CARPETA_SEQ%
echo.
echo  2. Ejecutar MPI
echo     Carpeta: %CARPETA_MPI%
echo.
echo  3. Ejecutar OpenMP (abre MSYS2)
echo     Carpeta: %CARPETA_OMP%
echo.
echo  4. Ejecutar HIBRIDA (MPI + OpenMP)
echo     Carpeta: %CARPETA_HIB%
echo.
echo  5. Ver METRICAS + GRAFICOS de rendimiento
echo  6. Cambiar rutas de las carpetas
echo  7. Conceptos
echo  8. Salir
echo.
set /p opcion=Elige una opcion (1-8): 

if "%opcion%"=="1" goto secuencial
if "%opcion%"=="2" goto mpi
if "%opcion%"=="3" goto openmp
if "%opcion%"=="4" goto hibrida
if "%opcion%"=="5" goto metricas
if "%opcion%"=="6" goto cambiar
if "%opcion%"=="7" goto ayuda
if "%opcion%"=="8" exit
echo Opcion invalida.
pause
goto menu

:metricas
cls
echo --- METRICAS + GRAFICOS DE RENDIMIENTO ---
echo.
echo Compilando analizador...
g++ analizar_metricas_con_explicacion.cpp -o analizar_metricas.exe -std=c++17 -O2 2>nul
if not exist analizar_metricas.exe (
    echo ERROR: No se pudo compilar. Asegurate de tener g++ en el PATH.
    echo Puedes compilar manualmente con:
    echo   g++ analizar_metricas_con_explicacion.cpp -o analizar_metricas.exe -std=c++17 -O2
    pause
    goto menu
)
echo.
echo Ejecutando analisis...
analizar_metricas.exe
echo.
echo.
echo === ARCHIVOS GENERADOS ===
dir /b *.svg 2>nul
echo.
echo Abriendo graficos (uno por uno)...
if exist grafico_comparacion.svg start "" grafico_comparacion.svg
timeout /t 2 /nobreak >nul
if exist grafico_openmp_loro.svg start "" grafico_openmp_loro.svg
timeout /t 2 /nobreak >nul
if exist grafico_openmp_etn.svg start "" grafico_openmp_etn.svg
echo.
echo Listo. Si algun grafico no se abrio, abrilo manualmente desde esta carpeta.
pause
goto menu

:cambiar
cls
echo --- CAMBIAR RUTAS ---
echo.
set /p CARPETA_SEQ=Nueva carpeta SECUENCIAL [%CARPETA_SEQ%]: 
set /p CARPETA_MPI=Nueva carpeta MPI [%CARPETA_MPI%]: 
set /p CARPETA_OMP=Nueva carpeta OpenMP [%CARPETA_OMP%]: 
set /p CARPETA_HIB=Nueva carpeta HIBRIDA [%CARPETA_HIB%]: 
echo.
echo Rutas actualizadas.
pause
goto menu

:ayuda
cls
echo ==========================================
echo            CONCEPTOS
echo ==========================================
echo.
echo VERSIONES:
echo   Secuencial  - Ejecuta en un solo nucleo, sin paralelismo.
echo   OpenMP      - Usa varios threads en memoria compartida.
echo   MPI         - Usa varios procesos en memoria distribuida.
echo   Hibrida     - Combina procesos MPI + threads OpenMP.
echo.
echo PARAMETROS:
echo   Filtro (3 o 5)    - Tamano del filtro de suavizado.
echo                       3 = mas rapido, 5 = mas suave.
echo   Niveles (3 o 9)   - Niveles de posterizado (reduccion de colores).
echo                       Menos niveles = mas efecto cartoon.
echo   Umbral (30-100)   - Sensibilidad de deteccion de bordes.
echo                       Menor = mas bordes, Mayor = menos bordes.
echo   Threads           - Cantidad de hilos (OpenMP). Probar 1, 2, 4, 8.
echo   Procesos          - Cantidad de procesos MPI.
echo.
echo ARCHIVOS GENERADOS:
echo   rdo_*.png              - Imagen con efecto cartoon.
echo   informe_*.txt          - Tiempos de cada ejecucion.
echo   metricas_rendimiento.txt - Analisis de speedup y eficiencia.
echo   grafico_*.svg          - Graficos comparativos (abrir con navegador).
echo.
echo METRICAS:
echo   Speedup    = Tiempo_secuencial / Tiempo_paralelo
echo   Eficiencia = (Speedup / threads_o_procesos) * 100%%
echo.
pause
goto menu



:secuencial
cls
echo --- SECUENCIAL ---
echo Carpeta: %CARPETA_SEQ%
if not exist "%CARPETA_SEQ%\cartoon.exe" (
    echo ERROR: No se encontro cartoon.exe
    pause
    goto menu
)
set /p img=Nombre de la imagen: 
set /p filtro=Filtro (3 o 5) [3]: 
set /p niveles=Niveles (3 o 9) [3]: 
set /p umbral=Umbral [50]: 
if "%filtro%"=="" set filtro=3
if "%niveles%"=="" set niveles=3
if "%umbral%"=="" set umbral=50
echo.
echo Ejecutando...
cd /d "%CARPETA_SEQ%"
cartoon.exe "%img%" %filtro% %niveles% %umbral%
cd /d "%~dp0"
echo.
echo Resultado: rdo_seq.png
if exist "%CARPETA_SEQ%\rdo_seq.png" (
    start "" "%CARPETA_SEQ%\rdo_seq.png"
)
pause
goto menu

:mpi
cls
echo --- MPI ---
echo Carpeta: %CARPETA_MPI%
if not exist "%CARPETA_MPI%\cartoon_mpi.exe" (
    echo ERROR: No se encontro cartoon_mpi.exe
    pause
    goto menu
)
set /p img=Nombre de la imagen: 
set /p procesos=Procesos MPI [4]: 
set /p filtro=Filtro (3 o 5) [3]: 
set /p niveles=Niveles (3 o 9) [3]: 
set /p umbral=Umbral [50]: 
if "%procesos%"=="" set procesos=4
if "%filtro%"=="" set filtro=3
if "%niveles%"=="" set niveles=3
if "%umbral%"=="" set umbral=50
echo.
echo Ejecutando con %procesos% procesos...
cd /d "%CARPETA_MPI%"
mpiexec -n %procesos% cartoon_mpi.exe "%img%" %filtro% %niveles% %umbral%
cd /d "%~dp0"
echo.
echo Resultado: rdo_mpi.png
if exist "%CARPETA_MPI%\rdo_mpi.png" (
    start "" "%CARPETA_MPI%\rdo_mpi.png"
)
pause
goto menu

:openmp
cls
echo --- OpenMP (MSYS2 MinGW64) ---
echo.
echo Se abrira MSYS2. Dentro vas a ver los comandos listos para copiar.
echo.
pause
start "" "C:\msys64\usr\bin\mintty.exe" -i /msys2.ico /mingw64.exe /usr/bin/bash --login -c ^
"echo ''; echo '========================================='; echo '  COMANDOS PARA OPENMP'; echo '========================================='; echo ''; echo '1. Entrar a la carpeta:'; echo '   cd /c/2026/CA/FinalMio/open_mp'; echo ''; echo '2. Compilar:'; echo '   g++ cartoon_openmp.cpp -o cartoon_openmp.exe -std=c++17 -O2 -fopenmp'; echo ''; echo '3. Ejecutar (cambia el ultimo numero = threads):'; echo '   ./cartoon_openmp.exe loro.jpg 3 3 50 1'; echo '   ./cartoon_openmp.exe loro.jpg 3 3 50 2'; echo '   ./cartoon_openmp.exe loro.jpg 3 3 50 4'; echo '   ./cartoon_openmp.exe loro.jpg 3 3 50 8'; echo ''; echo '========================================='; echo ''; exec bash -i"
goto menu

:hibrida
cls
echo --- HIBRIDA (MSYS2 MinGW64) ---
echo.
echo Se abrira MSYS2. Dentro vas a ver los comandos listos.
echo.
pause
start "" "C:\msys64\usr\bin\mintty.exe" -i /msys2.ico /mingw64.exe /usr/bin/bash --login -c ^
"echo ''; echo '========================================='; echo '  COMANDOS PARA HIBRIDA'; echo '========================================='; echo ''; echo '1. Entrar a la carpeta:'; echo '   cd /c/2026/CA/FinalMio/hibrida'; echo ''; echo '2. Compilar:'; echo '   g++ cartoon_hibrida.cpp -o cartoon_hibrida.exe -std=c++17 -O2 -fopenmp -I\"/c/Program Files (x86)/Microsoft SDKs/MPI/Include\" -L\"/c/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64\" -lmsmpi'; echo ''; echo '3. Ejecutar:'; echo '   ./cartoon_hibrida.exe loro.jpg 3 3 50 2'; echo ''; echo '========================================='; echo ''; exec bash -i"
goto menu
