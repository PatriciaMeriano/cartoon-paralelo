# Generador de Cartoon Paralelo

Implementacion de un filtro cartoon a partir de imagenes utilizando computacion paralela.

## Versiones implementadas

- Secuencial
- OpenMP (memoria compartida)
- MPI (memoria distribuida)
- Hibrida (MPI + OpenMP)

## Requisitos

- Compilador C++ (g++ o MSYS2 MinGW64)
- OpenMP (para las versiones paralelas)
- MS-MPI (para las versiones MPI e Hibrida)

## Compilacion

### Secuencial
g++ cartoon_secuencial.cpp -o cartoon.exe -std=c++17 -O2

### OpenMP
g++ cartoon_openmp.cpp -o cartoon_openmp.exe -std=c++17 -O2 -fopenmp

### MPI
g++ cartoon_mpi.cpp -o cartoon_mpi.exe -std=c++17 -O2 -I. -I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" -L"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x86" -lmsmpi

### Hibrida
g++ cartoon_hibrida.cpp -o cartoon_hibrida.exe -std=c++17 -O2 -fopenmp -I"/c/Program Files (x86)/Microsoft SDKs/MPI/Include" -L"/c/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64" -lmsmpi

## Ejecucion

### Secuencial
cartoon.exe imagen.jpg 3 3 50

### OpenMP
./cartoon_openmp.exe imagen.jpg 3 3 50 4

### MPI
mpiexec -n 4 cartoon_mpi.exe imagen.jpg 3 3 50

### Hibrida
./cartoon_hibrida.exe imagen.jpg 3 3 50 2

## Parametros

| Parametro | Descripcion | Valores recomendados |
|-----------|-------------|----------------------|
| filtro    | Tamano del filtro | 3 o 5 |
| niveles   | Niveles de posterizado | 3 o 9 |
| umbral    | Umbral de bordes | 30 - 100 |
| threads   | Cantidad de threads (OpenMP) | 1, 2, 4, 8 |

## Informes

Cada ejecucion genera un archivo de informe con los tiempos:
- informe_secuencial.txt
- informe_openmp.txt
- informe_mpi.txt
- informe_hibrida.txt

## Autor

Patricia Meriano