// cartoon_hibrida.cpp
// Versión Híbrida MPI + OpenMP
// Compila con: mpic++ cartoon_hibrida.cpp -o cartoon_hibrida.exe -std=c++17 -O2 -fopenmp
// Ejecuta con: mpiexec -n 4 cartoon_hibrida.exe imagen.jpg 3 3 50 2

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <fstream>

using namespace std;

// Estructura para representar una imagen en memoria
struct Image {
    int width, height, channels;
    vector<unsigned char> data;
    unsigned char& at(int x, int y, int c) { return data[(y * width + x) * channels + c]; }
    unsigned char at(int x, int y, int c) const { return data[(y * width + x) * channels + c]; }
};

// Carga una imagen desde disco (solo proceso MPI 0)
Image loadImage(const char* filename) {
    int w, h, c;
    unsigned char* data = stbi_load(filename, &w, &h, &c, 0);
    if (!data) { cerr << "Error cargando imagen\n"; exit(1); }
    Image img; img.width = w; img.height = h; img.channels = c;
    img.data.assign(data, data + w*h*c);
    stbi_image_free(data);
    return img;
}

// Guarda una imagen en disco como PNG (solo proceso MPI 0)
void saveImage(const char* filename, const Image& img) {
    stbi_write_png(filename, img.width, img.height, img.channels, img.data.data(), img.width * img.channels);
}

// ============================================================
// PROCESAMIENTO DE UN BLOQUE
// Esta funcion se ejecuta en CADA proceso MPI.
// Dentro, los bucles estan paralelizados con OpenMP (threads).
// ============================================================
void processBlock(vector<unsigned char>& block, int local_h, int w, int channels,
                  int filterSize, int levels, int thresh) {
    vector<unsigned char> gray(local_h * w);
    
    // OPENMP: cada thread convierte un rango de pixeles a gris
    #pragma omp parallel for
    for (int i = 0; i < local_h * w; i++) {
        int base = i * channels;
        gray[i] = static_cast<unsigned char>(0.299f*block[base] + 0.587f*block[base+1] + 0.114f*block[base+2]);
    }

    vector<unsigned char> blurred(gray.size());
    int k = filterSize / 2;
    
    // OPENMP: cada thread suaviza un conjunto de filas del bloque
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < local_h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0, count = 0;
            for (int ky = -k; ky <= k; ky++)
                for (int kx = -k; kx <= k; kx++) {
                    int ny = y + ky, nx = x + kx;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < local_h) {
                        sum += gray[ny*w + nx]; count++;
                    }
                }
            blurred[y*w + x] = static_cast<unsigned char>(sum / max(1, count));
        }
    }

    vector<float> mag(local_h * w, 0.0f);
    const int sx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    const int sy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    
    // OPENMP: cada thread calcula el gradiente Sobel de un conjunto de pixeles
    #pragma omp parallel for collapse(2)
    for (int y = 1; y < local_h-1; y++) {
        for (int x = 1; x < w-1; x++) {
            float gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ky++)
                for (int kx = -1; kx <= 1; kx++) {
                    int val = blurred[(y+ky)*w + (x+kx)];
                    gx += val * sx[ky+1][kx+1];
                    gy += val * sy[ky+1][kx+1];
                }
            mag[y*w + x] = sqrt(gx*gx + gy*gy);
        }
    }

    float maxv = 0;
    for (float v : mag) maxv = max(maxv, v);
    vector<unsigned char> edges(local_h * w, 0);
    if (maxv > 0) {
        // OPENMP: cada thread binariza un rango de bordes
        #pragma omp parallel for
        for (int i = 0; i < local_h*w; i++)
            edges[i] = (mag[i]/maxv*255 > thresh) ? 255 : 0;
    }

    float step = 256.0f / levels;
    // OPENMP: cada thread posteriza y dibuja bordes en su zona del bloque
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < local_h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y*w + x) * channels;
            if (edges[y*w + x] > 128) {
                for (int c = 0; c < channels; c++) block[idx+c] = 0;
            } else {
                for (int c = 0; c < channels; c++) {
                    unsigned char val = block[idx+c];
                    unsigned char q = static_cast<unsigned char>(floor(val/step)*step + step/2.0f);
                    block[idx+c] = min(255, (int)q);
                }
            }
        }
    }
}

// ============================================================
// FUNCION PRINCIPAL - VERSION HIBRIDA (MPI + OpenMP)
//
// MPI: divide la imagen en bloques de filas entre procesos.
//      Cada proceso tiene su propia memoria.
//
// OpenMP: dentro de cada proceso, los threads paralelizan
//         los bucles sobre el bloque local.
//
// Ejemplo: 4 procesos MPI x 2 threads OpenMP = 8 unidades de computo
// ============================================================
int main(int argc, char** argv) {
    // Inicializar MPI
    MPI_Init(&argc, &argv);
    // rank = numero de este proceso MPI (0, 1, 2, ...)
    // size = cantidad total de procesos MPI
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) cout << "Uso: mpiexec -n N cartoon_hibrida.exe imagen.jpg [filtro] [niveles] [umbral] [threads]\n";
        MPI_Finalize(); return 1;
    }

    string input = argv[1];
    int filterSize = (argc > 2) ? stoi(argv[2]) : 3;
    int levels = (argc > 3) ? stoi(argv[3]) : 3;
    int thresh = (argc > 4) ? stoi(argv[4]) : 50;
    // Cantidad de threads OpenMP por cada proceso MPI
    int threads = (argc > 5) ? stoi(argv[5]) : 2;

    // Configurar OpenMP: cada proceso MPI usara esta cantidad de threads
    omp_set_num_threads(threads);
    if (rank == 0) cout << "MPI procesos: " << size << " | OpenMP threads por proceso: " << threads << endl;

    int width=0, height=0, channels=0;
    vector<unsigned char> full_data;

    // MPI: solo el proceso 0 carga la imagen completa
    if (rank == 0) {
        Image img = loadImage(input.c_str());
        width = img.width; height = img.height; channels = img.channels;
        full_data = img.data;
        cout << "Imagen: " << width << "x" << height << endl;
    }

    // MPI: Broadcast - el proceso 0 envia las dimensiones a todos los procesos
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&channels, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // MPI: calcular cuantas filas le tocan a cada proceso
    int rows_per = height / size;
    int rem = height % size;
    int local_rows = rows_per + (rank < rem ? 1 : 0);

    vector<unsigned char> local(local_rows * width * channels);
    vector<int> sendcounts(size), displs(size);

    if (rank == 0) {
        int off = 0;
        for (int i = 0; i < size; i++) {
            int r = rows_per + (i < rem ? 1 : 0);
            sendcounts[i] = r * width * channels;
            displs[i] = off;
            off += sendcounts[i];
        }
    }

    // ============================================================
    // MPI: Scatterv - el proceso 0 divide la imagen y envia
    // a cada proceso su bloque de filas
    // ============================================================
    MPI_Scatterv(full_data.data(), sendcounts.data(), displs.data(), MPI_UNSIGNED_CHAR,
                 local.data(), local_rows*width*channels, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    // ============================================================
    // PROCESAMIENTO HIBRIDO
    // MPI: cada proceso trabaja sobre su bloque de filas
    // OpenMP: dentro de processBlock, los threads paralelizan los bucles
    // ============================================================
    auto t1 = chrono::high_resolution_clock::now();
    processBlock(local, local_rows, width, channels, filterSize, levels, thresh);
    auto t2 = chrono::high_resolution_clock::now();

    vector<unsigned char> result;

    if (rank == 0) result.resize(height * width * channels);

    // ============================================================
    // MPI: Gatherv - cada proceso envia su bloque procesado
    // al proceso 0, que los ensambla en la imagen final
    // ============================================================
    MPI_Gatherv(local.data(), local_rows*width*channels, MPI_UNSIGNED_CHAR,
                result.data(), sendcounts.data(), displs.data(), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    // MPI: solo el proceso 0 guarda el resultado y el informe
            if (rank == 0) {
        auto ms = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
        cout << "Tiempo: " << ms << " ms" << endl;

        // El nombre incluye procesos MPI y threads OpenMP
        string outputName = "rdo_hibrida_" + to_string(size) + "proc_" + to_string(threads) + "threads.png";

        Image out;
        out.width = width;
        out.height = height;
        out.channels = channels;
        out.data = result;
        saveImage(outputName.c_str(), out);
        cout << "Guardado: " << outputName << endl;

        // Abrir la imagen automáticamente (Windows)
        string cmd = "start \"\" \"" + outputName + "\"";
        system(cmd.c_str());

        // Informe
        ofstream informe("informe_hibrida.txt", ios::app);
        if (informe.is_open()) {
            informe << "==============================\n";
            informe << "Version: HIBRIDA (MPI + OpenMP)\n";
            informe << "Imagen: " << input << "\n";
            informe << "Procesos MPI: " << size << " | Threads por proceso: " << threads << "\n";
            informe << "Filtro: " << filterSize << " | Niveles: " << levels << " | Umbral: " << thresh << "\n";
            informe << "Tiempo: " << ms << " ms\n";
            informe << "Archivo generado: " << outputName << "\n";
            informe << "==============================\n\n";
            informe.close();
            cout << "Informe guardado: informe_hibrida.txt" << endl;
        }
    }

}