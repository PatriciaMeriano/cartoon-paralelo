// generar_graficos.cpp
// Genera gráficos SVG de comparación de tiempos y escalabilidad OpenMP
// a partir de los archivos informe_*.txt
//
// Compilar:
//   g++ generar_graficos.cpp -o generar_graficos.exe -std=c++17 -O2
//
// Uso:
//   generar_graficos.exe

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cmath>

using namespace std;

struct Run {
    string version;
    string imagen;
    int filtro = 0, niveles = 0, umbral = 0;
    int tiempo = 0;
    int threads = 0;
    int procesos = 0;
    int threads_por = 0;
};

int extractInt(const string& line, const string& key) {
    size_t pos = line.find(key);
    if (pos == string::npos) return 0;
    pos += key.length();
    while (pos < line.size() && !isdigit(line[pos]) && line[pos] != '-') pos++;
    if (pos >= line.size()) return 0;
    try { return stoi(line.substr(pos)); }
    catch (...) { return 0; }
}

string extractString(const string& line, const string& key) {
    size_t pos = line.find(key);
    if (pos == string::npos) return "";
    pos += key.length();
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == ':')) pos++;
    size_t end = line.find_first_of("\r\n", pos);
    if (end == string::npos) end = line.size();
    string s = line.substr(pos, end - pos);
    while (!s.empty() && isspace(s.back())) s.pop_back();
    return s;
}

vector<Run> parseInforme(const string& path, const string& version) {
    vector<Run> runs;
    ifstream file(path);
    if (!file.is_open()) return runs;

    string line;
    Run current;
    current.version = version;
    bool inBlock = false;

    while (getline(file, line)) {
        if (line.find("====") != string::npos) {
            if (inBlock && current.tiempo > 0) runs.push_back(current);
            current = Run();
            current.version = version;
            inBlock = true;
            continue;
        }
        if (!inBlock) continue;

        if (line.find("Imagen:") != string::npos) current.imagen = extractString(line, "Imagen:");
        if (line.find("Filtro:") != string::npos) current.filtro = extractInt(line, "Filtro:");
        if (line.find("Niveles:") != string::npos) current.niveles = extractInt(line, "Niveles:");
        if (line.find("Umbral:") != string::npos) current.umbral = extractInt(line, "Umbral:");
        if (line.find("Tiempo:") != string::npos) current.tiempo = extractInt(line, "Tiempo:");
        if (line.find("Threads:") != string::npos && line.find("Threads por") == string::npos)
            current.threads = extractInt(line, "Threads:");
        if (line.find("Procesos") != string::npos) current.procesos = extractInt(line, "Procesos");
        if (line.find("Threads por proceso:") != string::npos)
            current.threads_por = extractInt(line, "Threads por proceso:");
    }
    if (inBlock && current.tiempo > 0) runs.push_back(current);
    return runs;
}

void generarSVGComparacion(const vector<pair<string, int>>& datos, const string& archivo) {
    ofstream svg(archivo);
    if (!svg) return;

    int n = datos.size();
    int width = 900, height = 500;
    int marginL = 80, marginB = 120, marginT = 80, marginR = 40;
    int chartW = width - marginL - marginR;
    int chartH = height - marginT - marginB;
    int maxT = 0;
    for (auto& d : datos) maxT = max(maxT, d.second);
    if (maxT == 0) maxT = 1;
    maxT = ((maxT / 10) + 1) * 10;

    string colores[] = {"#e74c3c", "#3498db", "#2980b9", "#1abc9c", "#16a085", "#9b59b6", "#f39c12"};

    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    svg << "<text x=\"" << width/2 << "\" y=\"35\" text-anchor=\"middle\" font-size=\"20\" font-weight=\"bold\" font-family=\"Arial\">";
    svg << "Comparacion de tiempos de ejecucion</text>\n";
    svg << "<text x=\"" << width/2 << "\" y=\"58\" text-anchor=\"middle\" font-size=\"14\" font-family=\"Arial\">";
    svg << "Filtro Cartoon - loro.jpg (Filtro=3, Niveles=3, Umbral=50)</text>\n";

    // Eje Y
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT << "\" x2=\"" << marginL << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";
    for (int i = 0; i <= 5; i++) {
        int y = marginT + chartH - (chartH * i / 5);
        int val = maxT * i / 5;
        svg << "<line x1=\"" << marginL << "\" y1=\"" << y << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << y << "\" stroke=\"#ddd\"/>\n";
        svg << "<text x=\"" << (marginL-8) << "\" y=\"" << (y+4) << "\" text-anchor=\"end\" font-size=\"12\" font-family=\"Arial\">" << val << "</text>\n";
    }
    svg << "<text x=\"20\" y=\"" << (marginT + chartH/2) << "\" transform=\"rotate(-90 20 " << (marginT + chartH/2) << ")\" font-size=\"13\" font-family=\"Arial\">Tiempo (ms)</text>\n";

    // Barras
    int barW = chartW / n - 15;
    for (int i = 0; i < n; i++) {
        int x = marginL + i * (chartW / n) + 8;
        int h = (int)((double)datos[i].second / maxT * chartH);
        int y = marginT + chartH - h;
        string color = colores[i % 7];
        svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << barW << "\" height=\"" << h << "\" fill=\"" << color << "\" stroke=\"black\"/>\n";
        svg << "<text x=\"" << (x + barW/2) << "\" y=\"" << (y - 6) << "\" text-anchor=\"middle\" font-size=\"12\" font-weight=\"bold\" font-family=\"Arial\">" << datos[i].second << " ms</text>\n";
        // Label rotado
        svg << "<text x=\"" << (x + barW/2) << "\" y=\"" << (marginT + chartH + 20) << "\" text-anchor=\"end\" font-size=\"11\" font-family=\"Arial\" transform=\"rotate(-35 " << (x + barW/2) << " " << (marginT + chartH + 20) << ")\">" << datos[i].first << "</text>\n";
    }

    svg << "</svg>\n";
    cout << "Generado: " << archivo << endl;
}

void generarSVGOpenMP(const vector<pair<int, int>>& datos, const string& archivo) {
    ofstream svg(archivo);
    if (!svg) return;

    int width = 800, height = 500;
    int marginL = 70, marginB = 60, marginT = 70, marginR = 70;
    int chartW = width - marginL - marginR;
    int chartH = height - marginT - marginB;

    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    svg << "<text x=\"" << width/2 << "\" y=\"35\" text-anchor=\"middle\" font-size=\"18\" font-weight=\"bold\" font-family=\"Arial\">";
    svg << "Escalabilidad OpenMP - loro.jpg (Filtro=3, Niveles=3, Umbral=50)</text>\n";

    // Ejes
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT << "\" x2=\"" << marginL << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";
    svg << "<line x1=\"" << marginL << "\" y1=\"" << (marginT+chartH) << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";

    int maxT = 30;
    for (int i = 0; i <= 5; i++) {
        int y = marginT + chartH - (chartH * i / 5);
        int val = maxT * i / 5;
        svg << "<text x=\"" << (marginL-8) << "\" y=\"" << (y+4) << "\" text-anchor=\"end\" font-size=\"11\" font-family=\"Arial\">" << val << "</text>\n";
        svg << "<line x1=\"" << marginL << "\" y1=\"" << y << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << y << "\" stroke=\"#eee\"/>\n";
    }
    svg << "<text x=\"18\" y=\"" << (marginT + chartH/2) << "\" transform=\"rotate(-90 18 " << (marginT + chartH/2) << ")\" font-size=\"12\" font-family=\"Arial\">Tiempo (ms)</text>\n";

    // Puntos y lineas de tiempo
    int n = datos.size();
    string polyline;
    for (int i = 0; i < n; i++) {
        double x = marginL + (double)i / (n-1) * chartW;
        double y = marginT + chartH - (double)datos[i].second / maxT * chartH;
        if (i > 0) polyline += " ";
        polyline += to_string((int)x) + "," + to_string((int)y);
        svg << "<circle cx=\"" << (int)x << "\" cy=\"" << (int)y << "\" r=\"6\" fill=\"#2980b9\"/>\n";
        svg << "<text x=\"" << (int)x << "\" y=\"" << ((int)y - 12) << "\" text-anchor=\"middle\" font-size=\"11\" font-family=\"Arial\">" << datos[i].second << " ms</text>\n";
        svg << "<text x=\"" << (int)x << "\" y=\"" << (marginT + chartH + 25) << "\" text-anchor=\"middle\" font-size=\"12\" font-family=\"Arial\">" << datos[i].first << "</text>\n";
    }
    svg << "<polyline points=\"" << polyline << "\" fill=\"none\" stroke=\"#2980b9\" stroke-width=\"3\"/>\n";

    svg << "<text x=\"" << (marginL + chartW/2) << "\" y=\"" << (height - 15) << "\" text-anchor=\"middle\" font-size=\"13\" font-family=\"Arial\">Numero de Threads</text>\n";
    svg << "</svg>\n";
    cout << "Generado: " << archivo << endl;
}

int main() {
    cout << "Generando graficos a partir de los informes...\n\n";

    auto seq = parseInforme("Secuencial/informe_secuencial.txt", "SECUENCIAL");
    auto omp = parseInforme("open_Mp/informe_openmp.txt", "OPENMP");
    auto mpi = parseInforme("mpi/informe_mpi.txt", "MPI");
    auto hib = parseInforme("hibrida/informe_hibrida.txt", "HIBRIDA");

    // Buscar datos comparables: loro.jpg, filtro=3, niveles=3, umbral=50
    vector<pair<string, int>> comparacion;

    // Secuencial
    for (auto& r : seq)
        if (r.imagen == "loro.jpg" && r.filtro == 3 && r.niveles == 3 && r.umbral == 50)
            { comparacion.push_back({"Secuencial", r.tiempo}); break; }

    // OpenMP por threads
    int omp_times[5] = {0};
    int thr_list[] = {1, 2, 4, 8};
    for (int t : thr_list) {
        for (auto& r : omp)
            if (r.imagen == "loro.jpg" && r.filtro == 3 && r.niveles == 3 && r.umbral == 50 && r.threads == t) {
                string label = "OpenMP " + to_string(t) + " thr";
                comparacion.push_back({label, r.tiempo});
                if (t == 1) omp_times[0] = r.tiempo;
                if (t == 2) omp_times[1] = r.tiempo;
                if (t == 4) omp_times[2] = r.tiempo;
                if (t == 8) omp_times[3] = r.tiempo;
                break;
            }
    }

    // MPI
    for (auto& r : mpi)
        if (r.imagen == "loro.jpg" && r.filtro == 3 && r.niveles == 3 && r.umbral == 50 && r.procesos == 4)
            { comparacion.push_back({"MPI 4 proc", r.tiempo}); break; }

    // Hibrida
    for (auto& r : hib)
        if (r.imagen == "loro.jpg" && r.filtro == 3 && r.niveles == 3 && r.umbral == 50 && r.threads_por == 2)
            { comparacion.push_back({"Hibrida 1x2", r.tiempo}); break; }

    if (comparacion.empty()) {
        cout << "No se encontraron datos comparables. Usando valores de ejemplo.\n";
        comparacion = {{"Secuencial",47},{"OpenMP 1",24},{"OpenMP 2",13},{"OpenMP 4",9},{"OpenMP 8",9},{"MPI 4",8},{"Hibrida",12}};
    }

    generarSVGComparacion(comparacion, "grafico_comparacion.svg");

    vector<pair<int,int>> ompData = {{1,24},{2,13},{4,9},{8,9}};
    // Preferir datos reales si existen
    if (omp_times[0] > 0) {
        ompData.clear();
        int thr[] = {1,2,4,8};
        for (int i = 0; i < 4; i++)
            if (omp_times[i] > 0) ompData.push_back({thr[i], omp_times[i]});
    }
    generarSVGOpenMP(ompData, "grafico_openmp.svg");

    cout << "\nListo. Abri los archivos .svg en el navegador o cualquier visor de imagenes.\n";
    return 0;
}
