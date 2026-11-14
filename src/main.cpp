// Procesamiento paralelo con OpenMP de ventas multinacionales y conversión a USD.
// Uso: ./programa --input ventas.csv --output resumen_ventas_usd.csv --threads 8
#include <omp.h>

#include <cstdlib>
#include <iostream>
#include <string>

struct Opciones {
    std::string input;
    std::string output = "resumen_ventas_usd.csv";
    std::string errores = "errores.csv";
    int threads = 0;  // 0 = valor por defecto de OpenMP
};

static void uso(const char* prog) {
    std::cerr << "Uso: " << prog
              << " --input <ventas.csv> [--output <resumen.csv>] [--errors <errores.csv>] [--threads N]\n";
}

static bool parsear_args(int argc, char** argv, Opciones& op) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto siguiente = [&](std::string& dst) {
            if (i + 1 >= argc) return false;
            dst = argv[++i];
            return true;
        };
        std::string v;
        if (a == "--input") { if (!siguiente(op.input)) return false; }
        else if (a == "--output") { if (!siguiente(op.output)) return false; }
        else if (a == "--errors") { if (!siguiente(op.errores)) return false; }
        else if (a == "--threads") {
            if (!siguiente(v)) return false;
            op.threads = std::atoi(v.c_str());
            if (op.threads <= 0) return false;
        }
        else if (a == "-h" || a == "--help") return false;
        else { std::cerr << "Argumento desconocido: " << a << "\n"; return false; }
    }
    return !op.input.empty();
}

int main(int argc, char** argv) {
    Opciones op;
    if (!parsear_args(argc, argv, op)) { uso(argv[0]); return 1; }
    if (op.threads > 0) omp_set_num_threads(op.threads);

    const double t0 = omp_get_wtime();

    // TODO 1. Credenciales desde variables de entorno (CPYD_EMAIL, CPYD_APIKEY) y login -> JWT.
    // TODO 2. Leer y validar el archivo de ventas; filas inválidas -> errores.csv.
    // TODO 3. Pares únicos (país, fecha) y descarga paralela de tasas con caché.
    // TODO 4. Conversión a USD y acumulación paralela por (país, cliente).
    // TODO 5. Ordenar y escribir el CSV de salida.

    std::cout << "Hilos: " << omp_get_max_threads()
              << "  Tiempo total: " << (omp_get_wtime() - t0) << " s\n";
    return 0;
}
