// cartoon_secuencial.cpp
// Versión secuencial - guarda como rdo_seq.png
// Compila con: g++ cartoon_secuencial.cpp -o cartoon.exe -std=c++17 -O2

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <fstream>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

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

Image posterize(const Image& img, int levels) {
    Image result = img;
    float step = 256.0f / levels;
    
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            for (int c = 0; c < img.channels; c++) {
                unsigned char val = img.at(x, y, c);
                unsigned char q = static_cast<unsigned char>(floor(val / step) * step + step / 2.0f);
                result.at(x, y, c) = min(255, (int)q);
            }
        }
    }
    return result;
}

vector<unsigned char> toGrayscale(const Image& img) {
    vector<unsigned char> gray(img.width * img.height);
    for (int i = 0; i < img.width * img.height; i++) {
        int base = i * img.channels;
        float r = img.data[base];
        float g = img.data[base + 1];
        float b = img.data[base + 2];
        gray[i] = static_cast<unsigned char>(0.299f * r + 0.587f * g + 0.114f * b);
    }
    return gray;
}

vector<unsigned char> blur(const vector<unsigned char>& gray, int w, int h, int ksize) {
    vector<unsigned char> result(gray.size());
    int k = ksize / 2;
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0, count = 0;
            for (int ky = -k; ky <= k; ky++) {
                for (int kx = -k; kx <= k; kx++) {
                    int ny = y + ky;
                    int nx = x + kx;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        sum += gray[ny * w + nx];
                        count++;
                    }
                }
            }
            result[y * w + x] = static_cast<unsigned char>(sum / max(1, count));
        }
    }
    return result;
}

vector<unsigned char> sobel(const vector<unsigned char>& gray, int w, int h) {
    vector<float> mag(w * h, 0.0f);
    
    const int sx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    const int sy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};
    
    for (int y = 1; y < h-1; y++) {
        for (int x = 1; x < w-1; x++) {
            float gx = 0, gy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int val = gray[(y+ky)*w + (x+kx)];
                    gx += val * sx[ky+1][kx+1];
                    gy += val * sy[ky+1][kx+1];
                }
            }
            mag[y*w + x] = sqrt(gx*gx + gy*gy);
        }
    }
    
    float maxv = 0;
    for (float v : mag) maxv = max(maxv, v);
    vector<unsigned char> edges(w * h, 0);
    if (maxv > 0) {
        for (int i = 0; i < w*h; i++) {
            edges[i] = static_cast<unsigned char>(min(255.0f, mag[i] / maxv * 255.0f));
        }
    }
    return edges;
}

void threshold(vector<unsigned char>& edges, int thresh) {
    for (auto& p : edges) {
        p = (p > thresh) ? 255 : 0;
    }
}

Image generateCartoon(const Image& original, int filterSize, int levels, int thresh) {
    auto gray = toGrayscale(original);
    auto blurred = blur(gray, original.width, original.height, filterSize);
    auto edges = sobel(blurred, original.width, original.height);
    threshold(edges, thresh);
    
    Image poster = posterize(original, levels);
    Image cartoon = poster;
    
    for (int y = 0; y < original.height; y++) {
        for (int x = 0; x < original.width; x++) {
            if (edges[y * original.width + x] > 128) {
                for (int c = 0; c < cartoon.channels; c++) {
                    cartoon.at(x, y, c) = 0;
                }
            }
        }
    }
    return cartoon;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Uso: cartoon.exe imagen.jpg [filtro=3] [niveles=3] [umbral=50]" << endl;
        return 1;
    }
    
    string input = argv[1];
    int filterSize = (argc > 2) ? stoi(argv[2]) : 3;
    int levels = (argc > 3) ? stoi(argv[3]) : 3;
    int thresh = (argc > 4) ? stoi(argv[4]) : 50;
    
    cout << "Cargando " << input << "..." << endl;
    Image img = loadImage(input.c_str());
    cout << "Tamaño: " << img.width << "x" << img.height << " (" << img.channels << " canales)" << endl;
    
    auto start = chrono::high_resolution_clock::now();
    Image cartoon = generateCartoon(img, filterSize, levels, thresh);
    auto end = chrono::high_resolution_clock::now();
    
    auto ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "Tiempo: " << ms << " ms" << endl;
    
    saveImage("rdo_seq.png", cartoon);
    cout << "Guardado: rdo_seq.png" << endl;

    // Abrir la imagen automáticamente (Windows)
    system("start \"\" \"rdo_seq.png\"");

    // Informe
    ofstream informe("informe_secuencial.txt", ios::app);
    if (informe.is_open()) {
        informe << "==============================\n";
        informe << "Version: SECUENCIAL\n";
        informe << "Imagen: " << input << "\n";
        informe << "Filtro: " << filterSize << " | Niveles: " << levels << " | Umbral: " << thresh << "\n";
        informe << "Tiempo: " << ms << " ms\n";
        informe << "==============================\n\n";
        informe.close();
        cout << "Informe guardado: informe_secuencial.txt" << endl;
    } else {
        cout << "ERROR al crear el informe" << endl;
    }

    return 0;
}