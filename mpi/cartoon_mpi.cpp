// cartoon_mpi.cpp
// Versión MPI - guarda como rdo_mpi.png
// Compila con: mpic++ cartoon_mpi.cpp -o cartoon_mpi -std=c++17 -O2
// Ejecuta con: mpirun -np 4 ./cartoon_mpi imagen.jpg 3 3 50

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <mpi.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sal.h>

using namespace std;

// Estructura para representar una imagen en memoria
struct Image {
    int width, height, channels;
    vector<unsigned char> data;

    unsigned char& at(int x, int y, int c) {
        return data[(y * width + x) * channels + c];
    }
    unsigned char at(int x, int y, int c) const {
        return data[(y * width + x) * channels + c];
    }
};

// Carga una imagen desde disco (solo la ejecuta el proceso 0)
Image loadImage(const char* filename) {
    int w, h, c;
    unsigned char* data = stbi_load(filename, &w, &h, &c, 0);
    if (!data) {
        cerr << "Error cargando imagen: " << filename << endl;
        exit(1);
    }
    Image img;
    img.width = w;
    img.height = h;
    img.channels = c;
    img.data.assign(data, data + w * h * c);
    stbi_image_free(data);
    return img;
}

// Guarda una imagen en disco como PNG (solo la ejecuta el proceso 0)
void saveImage(const char* filename, const Image& img) {
    stbi_write_png(filename, img.width, img.height, img.channels, img.data.data(), img.width * img.channels);
}

// ============================================================
// PROCESAMIENTO DE UN BLOQUE (se ejecuta en PARALELO en cada proceso)
// Cada proceso MPI recibe una porcion de filas de la imagen y aplica
// el filtro cartoon solo sobre esas filas.
// ============================================================
// Aplica el filtro cartoon sobre un bloque de filas
// block = datos del bloque, local_h = cantidad de filas del bloque
void processBlock(vector<unsigned char>& block, int local_h, int w, int channels,
                  int filterSize, int levels, int thresh) {
    // 1. Convertir el bloque a escala de grises
    vector<unsigned char> gray(local_h * w);
    for (int i = 0; i < local_h * w; i++) {
        int base = i * channels;
        float r = block[base];
        float g = block[base + 1];
        float b = block[base + 2];
        gray[i] = static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);
    }

    // 2. Aplicar blur (suavizado) al bloque en grises
    vector<unsigned char> blurred(gray.size());
    int k = filterSize / 2;
    for (int y = 0; y < local_h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0, count = 0;
            for (int ky = -k; ky <= k; ky++) {
                for (int kx = -k; kx <= k; kx++) {
                    int ny = y + ky;
                    int nx = x + kx;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < local_h) {
                        sum += gray[ny * w + nx];
                        count++;
                    }
                }
            }
            blurred[y * w + x] = static_cast<unsigned char>(sum / max(1, count));
        }
    }

    // 3. Detectar bordes con Sobel sobre el bloque suavizado
    vector<unsigned char> edges(local_h * w, 0);
    const int sx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    const int sy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    float maxv = 0;
    vector<float> mag(local_h * w, 0.0f);

    for (int y = 1; y < local_h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            float gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int val = blurred[(y + ky) * w + (x + kx)];
                    gx += val * sx[ky + 1][kx + 1];
                    gy += val * sy[ky + 1][kx + 1];
                }
            }
            mag[y * w + x] = sqrt(gx * gx + gy * gy);
            maxv = max(maxv, mag[y * w + x]);
        }
    }

    for (int i = 0; i < local_h * w; i++) {
        if (maxv > 0)
            edges[i] = static_cast<unsigned char>(min(255.0f, mag[i] / maxv * 255.0f));
        edges[i] = (edges[i] > thresh) ? 255 : 0;
    }

    // 4. Posterizar (reducir colores) y dibujar bordes en negro
    float step = 256.0f / levels;
    for (int y = 0; y < local_h; y++) {
        for (int x = 0; x < w; x++) {
            if (edges[y * w + x] > 128) {
                for (int c = 0; c < channels; c++)
                    block[(y * w + x) * channels + c] = 0;
            } else {
                for (int c = 0; c < channels; c++) {
                    unsigned char val = block[(y * w + x) * channels + c];
                    unsigned char q = static_cast<unsigned char>(floor(val / step) * step + step / 2.0f);
                    block[(y * w + x) * channels + c] = min(255, (int)q);
                }
            }
        }
    }
}

// ============================================================
// FUNCION PRINCIPAL - VERSION MPI
// La imagen se divide en bloques de filas. Cada proceso procesa
// su bloque en paralelo y luego se juntan los resultados.
// ============================================================
int main(int argc, char** argv) {
    // Inicializar MPI
    MPI_Init(&argc, &argv);

    // rank = numero de este proceso (0, 1, 2, ...)
    // size = cantidad total de procesos
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0)
            cout << "Uso: mpirun -np N cartoon_mpi imagen.jpg [filtro=3] [niveles=3] [umbral=50]" << endl;
        // Finalizar MPI
    MPI_Finalize();
        return 1;
    }

    string input = argv[1];
    int filterSize = (argc > 2) ? stoi(argv[2]) : 3;
    int levels = (argc > 3) ? stoi(argv[3]) : 3;
    int thresh = (argc > 4) ? stoi(argv[4]) : 50;

    int width = 0, height = 0, channels = 0;
    vector<unsigned char> full_data;

    // Solo el proceso 0 guarda el resultado y el informe
    if (rank == 0) {
        Image img = loadImage(input.c_str());
        width = img.width;
        height = img.height;
        channels = img.channels;
        full_data = img.data;
        cout << "Imagen: " << width << "x" << height << " (" << channels << " canales)" << endl;
        cout << "Procesos MPI: " << size << endl;
    }

    // Enviar las dimensiones de la imagen a todos los procesos
    // (Broadcast: el proceso 0 envia, todos reciben)
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&channels, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calcular cuantas filas le tocan a cada proceso
    // Si la altura no es divisible, los primeros procesos reciben una fila extra
    int rows_per_proc = height / size;
    int remainder = height % size;
    int local_rows = rows_per_proc + (rank < remainder ? 1 : 0);
    int start_row = rank * rows_per_proc + min(rank, remainder);

    // Reservar memoria para el bloque local de este proceso
    vector<unsigned char> local_data(local_rows * width * channels);

    // Calcular cuantos bytes enviar a cada proceso y desde que posicion
    // (solo lo calcula el proceso 0)
    vector<int> sendcounts(size), displs(size);
    if (rank == 0) {
        int offset = 0;
        for (int i = 0; i < size; i++) {
            int rows = rows_per_proc + (i < remainder ? 1 : 0);
            sendcounts[i] = rows * width * channels;
            displs[i] = offset;
            offset += sendcounts[i];
        }
    }

    // ============================================================
    // DISTRIBUCION: Scatterv envia a cada proceso su bloque de filas
    // El proceso 0 divide full_data y reparte los pedazos
    // ============================================================
    MPI_Scatterv(full_data.data(), sendcounts.data(), displs.data(), MPI_UNSIGNED_CHAR,
                 local_data.data(), local_rows * width * channels, MPI_UNSIGNED_CHAR,
                 0, MPI_COMM_WORLD);

    // ============================================================
    // PROCESAMIENTO EN PARALELO
    // Cada proceso ejecuta processBlock sobre su bloque de filas
    // Esto ocurre simultaneamente en todos los procesos
    // ============================================================
    auto t1 = chrono::high_resolution_clock::now();  // Inicio medicion de tiempo
    processBlock(local_data, local_rows, width, channels, filterSize, levels, thresh);
    auto t2 = chrono::high_resolution_clock::now();

    // ============================================================
    // RECOLECCION: Gatherv junta los bloques procesados en el proceso 0
    // Cada proceso envia su bloque, el proceso 0 los ensambla
    // ============================================================
    vector<unsigned char> result;
    if (rank == 0) result.resize(height * width * channels);

        MPI_Gatherv(local_data.data(), local_rows * width * channels, MPI_UNSIGNED_CHAR,
                result.data(), sendcounts.data(), displs.data(), MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        auto ms = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
        cout << "Tiempo: " << ms << " ms" << endl;

        Image out;
        out.width = width;
        out.height = height;
        out.channels = channels;
        out.data = result;
        saveImage("rdo_mpi.png", out);
        cout << "Guardado: rdo_mpi.png" << endl;

        // Abrir la imagen automáticamente (Windows)
        system("start \"\" \"rdo_mpi.png\"");

        // Informe
        ofstream informe("informe_mpi.txt", ios::app);
        if (informe.is_open()) {
            informe << "==============================\n";
            informe << "Version: MPI\n";
            informe << "Imagen: " << input << "\n";
            informe << "Procesos: " << size << "\n";
            informe << "Filtro: " << filterSize << " | Niveles: " << levels << " | Umbral: " << thresh << "\n";
            informe << "Tiempo: " << ms << " ms\n";
            informe << "==============================\n\n";
            informe.close();
            cout << "Informe guardado: informe_mpi.txt" << endl;
        }
    }

    MPI_Finalize();
    return 0;
}