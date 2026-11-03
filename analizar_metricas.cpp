// analizar_metricas.cpp
// Analizador de metricas de rendimiento en C++
// Lee los archivos informe_*.txt de las 4 versiones y genera:
//   - Mejor tiempo absoluto
//   - Mejor escalabilidad
//   - Eficiencia
//   - Speedup respecto al secuencial
//   - Graficos SVG de comparacion y escalabilidad OpenMP
//
// Compilar:
//   g++ analizar_metricas.cpp -o analizar_metricas.exe -std=c++17 -O2
//
// Uso:
//   ./analizar_metricas.exe

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <iomanip>
#include <cmath>
#include <climits>

using namespace std;

struct Run {
    string version;
    string imagen;
    int filtro = 0;
    int niveles = 0;
    int umbral = 0;
    int tiempo = 0;          // ms
    int threads = 0;
    int procesos = 0;
    int threads_por = 0;
    int p = 1;
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
    if (!file.is_open()) {
        cerr << "No se pudo abrir: " << path << endl;
        return runs;
    }

    string line;
    Run current;
    current.version = version;
    bool inBlock = false;

    while (getline(file, line)) {
        if (line.find("====") != string::npos) {
            if (inBlock && current.tiempo > 0) {
                if (current.threads > 0)
                    current.p = current.threads;
                else if (current.procesos > 0 && current.threads_por > 0)
                    current.p = current.procesos * current.threads_por;
                else if (current.procesos > 0)
                    current.p = current.procesos;
                else
                    current.p = 1;
                runs.push_back(current);
            }
            current = Run();
            current.version = version;
            inBlock = true;
            continue;
        }
        if (!inBlock) continue;

        if (line.find("Imagen:") != string::npos)
            current.imagen = extractString(line, "Imagen:");
        if (line.find("Filtro:") != string::npos)
            current.filtro = extractInt(line, "Filtro:");
        if (line.find("Niveles:") != string::npos)
            current.niveles = extractInt(line, "Niveles:");
        if (line.find("Umbral:") != string::npos)
            current.umbral = extractInt(line, "Umbral:");
        if (line.find("Tiempo:") != string::npos)
            current.tiempo = extractInt(line, "Tiempo:");
        if (line.find("Threads:") != string::npos && line.find("Threads por") == string::npos)
            current.threads = extractInt(line, "Threads:");
        if (line.find("Procesos") != string::npos)
            current.procesos = extractInt(line, "Procesos");
        if (line.find("Threads por proceso:") != string::npos)
            current.threads_por = extractInt(line, "Threads por proceso:");
    }
    if (inBlock && current.tiempo > 0) {
        if (current.threads > 0)
            current.p = current.threads;
        else if (current.procesos > 0 && current.threads_por > 0)
            current.p = current.procesos * current.threads_por;
        else if (current.procesos > 0)
            current.p = current.procesos;
        else
            current.p = 1;
        runs.push_back(current);
    }
    return runs;
}

// ==================== GENERACION DE GRAFICOS SVG ====================

void generarSVGComparacion(const vector<pair<string, int>>& datos, const string& archivo) {
    ofstream svg(archivo);
    if (!svg) return;

    int n = (int)datos.size();
    if (n == 0) return;

    int width = 900, height = 520;
    int marginL = 90, marginB = 140, marginT = 80, marginR = 40;
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

    // Eje Y y grilla
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT << "\" x2=\"" << marginL << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";
    for (int i = 0; i <= 5; i++) {
        int y = marginT + chartH - (chartH * i / 5);
        int val = maxT * i / 5;
        svg << "<line x1=\"" << marginL << "\" y1=\"" << y << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << y << "\" stroke=\"#eee\"/>\n";
        svg << "<text x=\"" << (marginL-8) << "\" y=\"" << (y+4) << "\" text-anchor=\"end\" font-size=\"12\" font-family=\"Arial\">" << val << "</text>\n";
    }
    svg << "<text x=\"25\" y=\"" << (marginT + chartH/2) << "\" transform=\"rotate(-90 25 " << (marginT + chartH/2) << ")\" font-size=\"13\" font-family=\"Arial\">Tiempo (ms)</text>\n";

    // Barras
    int barW = max(20, chartW / n - 18);
    for (int i = 0; i < n; i++) {
        int x = marginL + i * (chartW / n) + 10;
        int h = (int)((double)datos[i].second / maxT * chartH);
        int y = marginT + chartH - h;
        string color = colores[i % 7];
        svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << barW << "\" height=\"" << h << "\" fill=\"" << color << "\" stroke=\"black\" stroke-width=\"0.5\"/>\n";
        svg << "<text x=\"" << (x + barW/2) << "\" y=\"" << (y - 6) << "\" text-anchor=\"middle\" font-size=\"12\" font-weight=\"bold\" font-family=\"Arial\">" << datos[i].second << " ms</text>\n";
        svg << "<text x=\"" << (x + barW/2) << "\" y=\"" << (marginT + chartH + 25) << "\" text-anchor=\"end\" font-size=\"11\" font-family=\"Arial\" transform=\"rotate(-40 " << (x + barW/2) << " " << (marginT + chartH + 25) << ")\">" << datos[i].first << "</text>\n";
    }

    svg << "</svg>\n";
    cout << "  -> " << archivo << endl;
}

void generarSVGOpenMP(const vector<pair<int, int>>& datos, const string& archivo) {
    ofstream svg(archivo);
    if (!svg) return;

    int n = (int)datos.size();
    if (n < 2) return;

    int width = 800, height = 500;
    int marginL = 70, marginB = 70, marginT = 80, marginR = 50;
    int chartW = width - marginL - marginR;
    int chartH = height - marginT - marginB;

    int maxT = 0;
    for (auto& d : datos) maxT = max(maxT, d.second);
    if (maxT == 0) maxT = 1;
    maxT = ((maxT / 5) + 1) * 5;

    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    svg << "<text x=\"" << width/2 << "\" y=\"35\" text-anchor=\"middle\" font-size=\"18\" font-weight=\"bold\" font-family=\"Arial\">";
    svg << "Escalabilidad OpenMP - loro.jpg (Filtro=3, Niveles=3, Umbral=50)</text>\n";

    // Ejes
    svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT << "\" x2=\"" << marginL << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";
    svg << "<line x1=\"" << marginL << "\" y1=\"" << (marginT+chartH) << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";

    for (int i = 0; i <= 5; i++) {
        int y = marginT + chartH - (chartH * i / 5);
        int val = maxT * i / 5;
        svg << "<text x=\"" << (marginL-8) << "\" y=\"" << (y+4) << "\" text-anchor=\"end\" font-size=\"11\" font-family=\"Arial\">" << val << "</text>\n";
        svg << "<line x1=\"" << marginL << "\" y1=\"" << y << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << y << "\" stroke=\"#eee\"/>\n";
    }
    svg << "<text x=\"20\" y=\"" << (marginT + chartH/2) << "\" transform=\"rotate(-90 20 " << (marginT + chartH/2) << ")\" font-size=\"12\" font-family=\"Arial\">Tiempo (ms)</text>\n";

    // Linea y puntos
    string polyline;
    for (int i = 0; i < n; i++) {
        double x = marginL + (double)i / (n - 1) * chartW;
        double y = marginT + chartH - (double)datos[i].second / maxT * chartH;
        if (i > 0) polyline += " ";
        polyline += to_string((int)x) + "," + to_string((int)y);
        svg << "<circle cx=\"" << (int)x << "\" cy=\"" << (int)y << "\" r=\"6\" fill=\"#2980b9\" stroke=\"white\" stroke-width=\"2\"/>\n";
        svg << "<text x=\"" << (int)x << "\" y=\"" << ((int)y - 12) << "\" text-anchor=\"middle\" font-size=\"11\" font-weight=\"bold\" font-family=\"Arial\">" << datos[i].second << " ms</text>\n";
        svg << "<text x=\"" << (int)x << "\" y=\"" << (marginT + chartH + 25) << "\" text-anchor=\"middle\" font-size=\"12\" font-family=\"Arial\">" << datos[i].first << "</text>\n";
    }
    svg << "<polyline points=\"" << polyline << "\" fill=\"none\" stroke=\"#2980b9\" stroke-width=\"3\"/>\n";

    svg << "<text x=\"" << (marginL + chartW/2) << "\" y=\"" << (height - 15) << "\" text-anchor=\"middle\" font-size=\"13\" font-family=\"Arial\">Numero de Threads</text>\n";
    svg << "</svg>\n";
    cout << "  -> " << archivo << endl;
}

void generarGraficos(const vector<Run>& seq, const vector<Run>& omp, 
                     const vector<Run>& mpi, const vector<Run>& hib) {
    cout << "\nGenerando graficos SVG...\n";

    // --- Comparacion (barras agrupadas) ---
    vector<string> labels = {"Secuencial", "OpenMP 1", "OpenMP 2", "OpenMP 4", "OpenMP 8", "MPI 4", "Hibrida"};
    vector<int> tLoro(7, 0), tEtn(7, 0);

    // Secuencial
    for (auto& r : seq) {
        if (r.imagen == "loro.jpg" && r.filtro == 3 && r.niveles == 3 && (tLoro[0] == 0 || r.tiempo < tLoro[0])) tLoro[0] = r.tiempo;
        if (r.imagen == "etn.jpg"  && r.filtro == 3 && r.niveles == 3 && (tEtn[0] == 0 || r.tiempo < tEtn[0]))  tEtn[0] = r.tiempo;
    }
    // OpenMP
    int thr[] = {1,2,4,8};
    for (int i = 0; i < 4; i++) {
        for (auto& r : omp) {
            if (r.imagen == "loro.jpg" && r.threads == thr[i] && r.filtro == 3 && r.niveles == 3 && (tLoro[1+i] == 0 || r.tiempo < tLoro[1+i])) tLoro[1+i] = r.tiempo;
            if (r.imagen == "etn.jpg"  && r.threads == thr[i] && r.filtro == 3 && r.niveles == 3 && (tEtn[1+i] == 0 || r.tiempo < tEtn[1+i]))  tEtn[1+i] = r.tiempo;
        }
    }
    // MPI
    for (auto& r : mpi) {
        if (r.imagen == "loro.jpg" && r.procesos == 4 && (tLoro[5] == 0 || r.tiempo < tLoro[5])) tLoro[5] = r.tiempo;
        if (r.imagen == "etn.jpg"  && r.procesos == 4 && (tEtn[5] == 0 || r.tiempo < tEtn[5]))  tEtn[5] = r.tiempo;
    }
    // Hibrida
    for (auto& r : hib) {
        if (r.imagen == "loro.jpg" && r.threads_por == 2 && (tLoro[6] == 0 || r.tiempo < tLoro[6])) tLoro[6] = r.tiempo;
        if (r.imagen == "etn.jpg"  && r.threads_por == 2 && (tEtn[6] == 0 || r.tiempo < tEtn[6]))  tEtn[6] = r.tiempo;
    }

    // Generar SVG comparacion
    ofstream svg("grafico_comparacion.svg");
    if (svg) {
        int n = 7;
        int width = 1050, height = 560;
        int marginL = 90, marginB = 160, marginT = 80, marginR = 30;
        int chartW = width - marginL - marginR;
        int chartH = height - marginT - marginB;
        int maxT = 10;
        for (int v : tLoro) maxT = max(maxT, v);
        for (int v : tEtn) maxT = max(maxT, v);
        maxT = ((maxT/10)+1)*10;

        svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        svg << "<svg width=\"" << width << "\" height=\"" << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
        svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
        svg << "<text x=\"" << width/2 << "\" y=\"35\" text-anchor=\"middle\" font-size=\"20\" font-weight=\"bold\" font-family=\"Arial\">Comparacion de tiempos</text>\n";
        svg << "<text x=\"" << width/2 << "\" y=\"58\" text-anchor=\"middle\" font-size=\"14\" font-family=\"Arial\">loro.jpg (azul) vs etn.jpg (naranja)</text>\n";

        // Eje Y
        svg << "<line x1=\"" << marginL << "\" y1=\"" << marginT << "\" x2=\"" << marginL << "\" y2=\"" << (marginT+chartH) << "\" stroke=\"black\"/>\n";
        for (int i = 0; i <= 5; i++) {
            int y = marginT + chartH - (chartH * i / 5);
            svg << "<line x1=\"" << marginL << "\" y1=\"" << y << "\" x2=\"" << (marginL+chartW) << "\" y2=\"" << y << "\" stroke=\"#eee\"/>\n";
            svg << "<text x=\"" << (marginL-8) << "\" y=\"" << (y+4) << "\" text-anchor=\"end\" font-size=\"12\" font-family=\"Arial\">" << (maxT*i/5) << "</text>\n";
        }

        int groupW = chartW / n;
        int barW = groupW / 3;
        for (int i = 0; i < n; i++) {
            int baseX = marginL + i * groupW + 15;

            if (tLoro[i] > 0) {
                int h = (int)((double)tLoro[i] / maxT * chartH);
                int y = marginT + chartH - h;
                svg << "<rect x=\"" << baseX << "\" y=\"" << y << "\" width=\"" << barW << "\" height=\"" << h << "\" fill=\"#3498db\" stroke=\"black\"/>\n";
                svg << "<text x=\"" << (baseX+barW/2) << "\" y=\"" << (y-4) << "\" text-anchor=\"middle\" font-size=\"10\" font-family=\"Arial\">" << tLoro[i] << "</text>\n";
            }
            if (tEtn[i] > 0) {
                int h = (int)((double)tEtn[i] / maxT * chartH);
                int y = marginT + chartH - h;
                int x2 = baseX + barW + 6;
                svg << "<rect x=\"" << x2 << "\" y=\"" << y << "\" width=\"" << barW << "\" height=\"" << h << "\" fill=\"#e67e22\" stroke=\"black\"/>\n";
                svg << "<text x=\"" << (x2+barW/2) << "\" y=\"" << (y-4) << "\" text-anchor=\"middle\" font-size=\"10\" font-family=\"Arial\">" << tEtn[i] << "</text>\n";
            }
            svg << "<text x=\"" << (baseX+barW) << "\" y=\"" << (marginT+chartH+35) << "\" text-anchor=\"end\" font-size=\"11\" font-family=\"Arial\" transform=\"rotate(-40 " << (baseX+barW) << " " << (marginT+chartH+35) << ")\">" << labels[i] << "</text>\n";
        }

        // Leyenda
        svg << "<rect x=\"880\" y=\"75\" width=\"18\" height=\"18\" fill=\"#3498db\"/>\n";
        svg << "<text x=\"905\" y=\"89\" font-size=\"13\" font-family=\"Arial\">loro.jpg</text>\n";
        svg << "<rect x=\"880\" y=\"100\" width=\"18\" height=\"18\" fill=\"#e67e22\"/>\n";
        svg << "<text x=\"905\" y=\"114\" font-size=\"13\" font-family=\"Arial\">etn.jpg</text>\n";
        svg << "</svg>\n";
        cout << "  -> grafico_comparacion.svg\n";
    }

    // --- OpenMP loro ---
    vector<pair<int,int>> ompL;
    for (int t : {1,2,4,8}) {
        for (auto& r : omp)
            if (r.imagen == "loro.jpg" && r.threads == t && r.filtro == 3 && r.niveles == 3) {
                ompL.push_back({t, r.tiempo});
                break;
            }
    }
    if (!ompL.empty()) generarSVGOpenMP(ompL, "grafico_openmp_loro.svg");

    // --- OpenMP etn ---
    vector<pair<int,int>> ompE;
    for (int t : {1,2,4,8}) {
        for (auto& r : omp)
            if (r.imagen == "etn.jpg" && r.threads == t && r.filtro == 3 && r.niveles == 3) {
                ompE.push_back({t, r.tiempo});
                break;
            }
    }
    if (!ompE.empty()) generarSVGOpenMP(ompE, "grafico_openmp_etn.svg");

    cout << "\nArchivos generados:\n";
    cout << "  - grafico_comparacion.svg\n";
    if (!ompL.empty()) cout << "  - grafico_openmp_loro.svg\n";
    if (!ompE.empty()) cout << "  - grafico_openmp_etn.svg\n";
}

// ==================== REPORTE DE METRICAS ====================

void generarReporte(const vector<Run>& all) {
    ofstream out("metricas_rendimiento.txt");
    if (!out) {
        cerr << "No se pudo crear metricas_rendimiento.txt" << endl;
        return;
    }

    auto printBoth = [&](const string& s) {
        cout << s << endl;
        out << s << endl;
    };

    printBoth("======================================================================");
    printBoth("   ANALISIS DE RENDIMIENTO - GENERADOR DE CARTOON");
    printBoth("======================================================================");
    printBoth("");
    printBoth("Total de ejecuciones analizadas: " + to_string(all.size()));
    printBoth("");

    if (all.empty()) {
        printBoth("No hay datos.");
        return;
    }

    // Mejor tiempo absoluto
    auto best = min_element(all.begin(), all.end(),
                            [](const Run& a, const Run& b) { return a.tiempo < b.tiempo; });

    printBoth("1. MEJOR TIEMPO ABSOLUTO");
    printBoth("----------------------------------------");
    printBoth("   Tiempo: " + to_string(best->tiempo) + " ms");
    printBoth("   Version: " + best->version);
    printBoth("   Imagen: " + best->imagen);
    printBoth("   Parametros: filtro=" + to_string(best->filtro) +
              " niveles=" + to_string(best->niveles) +
              " umbral=" + to_string(best->umbral));
    if (best->threads) printBoth("   Threads: " + to_string(best->threads));
    if (best->procesos) printBoth("   Procesos MPI: " + to_string(best->procesos));
    if (best->threads_por) printBoth("   Threads por proceso: " + to_string(best->threads_por));
    printBoth("   Unidades de computo (p): " + to_string(best->p));
    printBoth("");

    // Mejor por version
    printBoth("2. MEJOR TIEMPO POR VERSION");
    printBoth("----------------------------------------");
    vector<string> versiones = {"SECUENCIAL", "OPENMP", "MPI", "HIBRIDA"};
    for (const auto& ver : versiones) {
        int bestT = INT_MAX;
        int bestP = 1;
        for (const auto& r : all) {
            if (r.version == ver && r.tiempo < bestT) {
                bestT = r.tiempo;
                bestP = r.p;
            }
        }
        if (bestT != INT_MAX) {
            ostringstream oss;
            oss << "   " << left << setw(12) << ver << " : " << right << setw(4) << bestT
                << " ms  (p~" << bestP << ")";
            printBoth(oss.str());
        }
    }
    printBoth("");

    // Referencia secuencial
    int seqRef = 0;
    for (const auto& r : all) {
        if (r.version == "SECUENCIAL" && r.imagen == "loro.jpg" &&
            r.filtro == 3 && r.niveles == 3 && (r.umbral == 35 || r.umbral == 50)) {
            if (seqRef == 0 || r.tiempo < seqRef) seqRef = r.tiempo;
        }
    }
    if (seqRef == 0) {
        for (const auto& r : all)
            if (r.version == "SECUENCIAL" && (seqRef == 0 || r.tiempo < seqRef))
                seqRef = r.tiempo;
    }

    if (seqRef > 0) {
        printBoth("3. SPEEDUP Y EFICIENCIA (referencia secuencial = " + to_string(seqRef) + " ms)");
        printBoth("----------------------------------------");
        printBoth("   Version      | Tiempo | Speedup | Eficiencia");
        printBoth("   ------------- | ------ | ------- | ----------");

        for (const auto& ver : versiones) {
            if (ver == "SECUENCIAL") continue;
            int bestT = INT_MAX;
            int bestP = 1;
            for (const auto& r : all) {
                if (r.version == ver && r.tiempo < bestT) {
                    bestT = r.tiempo;
                    bestP = r.p;
                }
            }
            if (bestT != INT_MAX && bestT > 0) {
                double S = (double)seqRef / bestT;
                double E = (S / bestP) * 100.0;
                ostringstream line;
                line << "   " << left << setw(13) << ver << " | " << right << setw(6) << bestT
                     << " | " << fixed << setprecision(2) << setw(7) << S << "x | "
                     << fixed << setprecision(1) << setw(8) << E << "%";
                printBoth(line.str());
            }
        }
        printBoth("");
    }

    // Escalabilidad OpenMP
    printBoth("4. ESCALABILIDAD OPENMP");
    printBoth("----------------------------------------");
    map<int, int> ompBest; // p -> mejor tiempo
    for (const auto& r : all) {
        if (r.version == "OPENMP" && r.threads > 0) {
            if (ompBest.find(r.threads) == ompBest.end() || r.tiempo < ompBest[r.threads])
                ompBest[r.threads] = r.tiempo;
        }
    }
    if (!ompBest.empty() && seqRef > 0) {
        printBoth("   Threads | Tiempo (ms) | Speedup | Eficiencia");
        printBoth("   -------- | ----------- | ------- | ----------");
        double bestEff = 0;
        int bestEffP = 0;
        for (auto& kv : ompBest) {
            int p = kv.first;
            int tmin = kv.second;
            double S = (double)seqRef / tmin;
            double E = (S / p) * 100.0;
            ostringstream line;
            line << "   " << setw(8) << p << " | " << setw(11) << tmin << " | "
                 << fixed << setprecision(2) << setw(7) << S << "x | "
                 << fixed << setprecision(1) << setw(8) << E << "%";
            printBoth(line.str());
            if (p > 1 && E > bestEff) {
                bestEff = E;
                bestEffP = p;
            }
        }
        printBoth("");
        if (bestEffP > 0)
            printBoth("   Mejor escalabilidad OpenMP: " + to_string(bestEffP) + " threads");
    } else {
        printBoth("   No hay suficientes datos de OpenMP.");
    }
    printBoth("");

    printBoth("   Observaciones:");
    printBoth("   - Speedups superlineales son normales en problemas pequenos");
    printBoth("     (mejor uso de cache, menos overhead relativo).");
    printBoth("   - La eficiencia baja al aumentar mucho el numero de threads");
    printBoth("     por overhead de sincronizacion y tamano del problema.");
    printBoth("");
    printBoth("======================================================================");
    printBoth("Generado automaticamente a partir de los archivos informe_*.txt");
    printBoth("======================================================================");

    out.close();
    cout << "\nInforme guardado en: metricas_rendimiento.txt" << endl;
}

int main() {
    cout << "Analizando archivos de informe..." << endl;

    auto seq  = parseInforme("Secuencial/informe_secuencial.txt", "SECUENCIAL");
    auto omp  = parseInforme("open_Mp/informe_openmp.txt", "OPENMP");
    auto mpi  = parseInforme("mpi/informe_mpi.txt", "MPI");
    auto hib  = parseInforme("hibrida/informe_hibrida.txt", "HIBRIDA");

    cout << "SECUENCIAL: " << seq.size() << " ejecuciones" << endl;
    cout << "OPENMP:     " << omp.size() << " ejecuciones" << endl;
    cout << "MPI:        " << mpi.size() << " ejecuciones" << endl;
    cout << "HIBRIDA:    " << hib.size() << " ejecuciones" << endl;

    vector<Run> all;
    all.insert(all.end(), seq.begin(), seq.end());
    all.insert(all.end(), omp.begin(), omp.end());
    all.insert(all.end(), mpi.begin(), mpi.end());
    all.insert(all.end(), hib.begin(), hib.end());

    generarReporte(all);
    generarGraficos(seq, omp, mpi, hib);

    cout << "\nListo. Abri los archivos .svg en el navegador para ver los graficos.\n";
    return 0;
}
