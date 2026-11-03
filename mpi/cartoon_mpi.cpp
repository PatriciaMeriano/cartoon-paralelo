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

void saveImage(const char* filename, const Image& img) {
    stbi_write_png(filename, img.width, img.height, img.channels, img.data.data(), img.width * img.channels);
}

// Funciones de procesamiento (igual que la secuencial, pero sobre un bloque)
void processBlock(vector<unsigned char>& block, int local_h, int w, int channels,
                  int filterSize, int levels, int thresh) {
    // Convertir bloque a gris
    vector<unsigned char> gray(local_h * w);
    for (int i = 0; i < local_h * w; i++) {
        int base = i * channels;
        float r = block[base];
        float g = block[base + 1];
        float b = block[base + 2];
        gray[i] = static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);
    }

    // Blur simple
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

    // Sobel (simplificado)
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

    // Posterizar + aplicar bordes negros
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

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0)
            cout << "Uso: mpirun -np N cartoon_mpi imagen.jpg [filtro=3] [niveles=3] [umbral=50]" << endl;
        MPI_Finalize();
        return 1;
    }

    string input = argv[1];
    int filterSize = (argc > 2) ? stoi(argv[2]) : 3;
    int levels = (argc > 3) ? stoi(argv[3]) : 3;
    int thresh = (argc > 4) ? stoi(argv[4]) : 50;

    int width = 0, height = 0, channels = 0;
    vector<unsigned char> full_data;

    if (rank == 0) {
        Image img = loadImage(input.c_str());
        width = img.width;
        height = img.height;
        channels = img.channels;
        full_data = img.data;
        cout << "Imagen: " << width << "x" << height << " (" << channels << " canales)" << endl;
        cout << "Procesos MPI: " << size << endl;
    }

    // Broadcast dimensiones
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&channels, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Dividir filas
    int rows_per_proc = height / size;
    int remainder = height % size;
    int local_rows = rows_per_proc + (rank < remainder ? 1 : 0);
    int start_row = rank * rows_per_proc + min(rank, remainder);

    // Preparar datos locales (con un poco de halo simple: se manda el bloque completo por simplicidad)
    vector<unsigned char> local_data(local_rows * width * channels);

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

    MPI_Scatterv(full_data.data(), sendcounts.data(), displs.data(), MPI_UNSIGNED_CHAR,
                 local_data.data(), local_rows * width * channels, MPI_UNSIGNED_CHAR,
                 0, MPI_COMM_WORLD);

    // Procesar bloque local
    auto t1 = chrono::high_resolution_clock::now();
    processBlock(local_data, local_rows, width, channels, filterSize, levels, thresh);
    auto t2 = chrono::high_resolution_clock::now();

    // Recoger resultados
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