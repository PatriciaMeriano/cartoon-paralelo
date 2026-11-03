# Generador de Cartoon Paralelo

Implementación de un filtro cartoon a partir de imágenes utilizando computación paralela.  
Proyecto que compara distintas estrategias de paralelización:

- **Secuencial**
- **OpenMP** (memoria compartida)
- **MPI** (memoria distribuida)
- **Híbrida** (MPI + OpenMP)

---

## Estructura del proyecto

```
FinalMio/
├── menu.bat                  ← Menú principal (recomendado)
├── analizar_metricas.cpp     ← Analizador de métricas en C++
├── README.md
├── Secuencial/
│   ├── cartoon_secuencial.cpp
│   ├── stb_image.h
│   ├── stb_image_write.h
│   ├── loro.jpg / etn.jpg
│   └── informe_secuencial.txt
├── open_Mp/
│   ├── cartoon_openmp.cpp
│   └── ...
├── mpi/
│   ├── cartoon_mpi.cpp
│   └── ...
└── hibrida/
    ├── cartoon_hibrida.cpp
    └── ...
```

---

## Requisitos

- Compilador C++ con soporte C++17 (g++ o MSYS2 MinGW64)
- OpenMP (incluido normalmente con g++)
- MS-MPI (solo para las versiones MPI e Híbrida)
- Windows (el menú está hecho en `.bat`)

---

## Cómo usar (recomendado)

1. Ejecuta `menu.bat`
2. Elige la versión que quieras correr (1 a 4)
3. Para ver las métricas de rendimiento (Speedup, Eficiencia, Escalabilidad) elige la opción **5**

El menú se encarga de cambiar de carpeta y ejecutar el programa correspondiente.

---

## Compilación manual

### Secuencial
```bash
cd Secuencial
g++ cartoon_secuencial.cpp -o cartoon.exe -std=c++17 -O2
```

### OpenMP
```bash
cd open_Mp
g++ cartoon_openmp.cpp -o cartoon_openmp.exe -std=c++17 -O2 -fopenmp
```

### MPI
```bash
cd mpi
g++ cartoon_mpi.cpp -o cartoon_mpi.exe -std=c++17 -O2 -I. -I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" -L"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x86" -lmsmpi
```

### Híbrida
```bash
cd hibrida
g++ cartoon_hibrida.cpp -o cartoon_hibrida.exe -std=c++17 -O2 -fopenmp -I"/c/Program Files (x86)/Microsoft SDKs/MPI/Include" -L"/c/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64" -lmsmpi
```

### Analizador de métricas
```bash
g++ analizar_metricas.cpp -o analizar_metricas.exe -std=c++17 -O2
```

---

## Ejecución manual

| Versión     | Comando ejemplo                                      |
|-------------|------------------------------------------------------|
| Secuencial  | `cartoon.exe loro.jpg 3 3 50`                        |
| OpenMP      | `cartoon_openmp.exe loro.jpg 3 3 50 4`               |
| MPI         | `mpiexec -n 4 cartoon_mpi.exe loro.jpg 3 3 50`       |
| Híbrida     | `cartoon_hibrida.exe loro.jpg 3 3 50 2`              |

### Parámetros

| Parámetro | Descripción                  | Valores recomendados |
|-----------|------------------------------|----------------------|
| filtro    | Tamaño del filtro            | 3 o 5                |
| niveles   | Niveles de posterizado       | 3 o 9                |
| umbral    | Umbral de detección de bordes| 30 - 100             |
| threads   | Cantidad de threads (OpenMP) | 1, 2, 4, 8           |

---

## Análisis de rendimiento

El programa `analizar_metricas.cpp` lee automáticamente los archivos:

- `Secuencial/informe_secuencial.txt`
- `open_Mp/informe_openmp.txt`
- `mpi/informe_mpi.txt`
- `hibrida/informe_hibrida.txt`

Y genera un resumen con:

- Mejor tiempo absoluto
- Mejor tiempo por versión
- Speedup (ganancia respecto al secuencial)
- Eficiencia
- Escalabilidad (tabla de OpenMP)

El resultado se guarda en `metricas_rendimiento.txt`.

También se puede ejecutar desde el menú (opción 5).

---

## Autor

Patricia Meriano
