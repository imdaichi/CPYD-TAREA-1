#include "cliente_http.h"
#include "configuracion_entorno.h"
#include "error.h"
#include "error_http.h"
#include "procesador_ventas.h"
#include "tasa_cambio.h"

#include <curl/curl.h>
#include <iomanip>
#include <iostream>
#include <omp.h>
#include <string>

namespace {
struct ArgumentosCli {
    std::string ruta_entrada = "ventas.csv";
    std::string ruta_salida = "resumen_ventas_usd.csv";
    std::string ruta_errores = "errores.csv";
    int num_hilos = omp_get_max_threads();
};

ArgumentosCli parsearArgumentos(int argc, char** argv) {
    ArgumentosCli args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            args.ruta_entrada = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            args.ruta_salida = argv[++i];
        } else if (arg == "--errors" && i + 1 < argc) {
            args.ruta_errores = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            args.num_hilos = std::stoi(argv[++i]);
            if (args.num_hilos <= 0) {
                throw Error("El numero de hilos (--threads) debe ser mayor a 0");
            }
        } else {
            throw Error("Argumento desconocido o incompleto: " + arg
                + "\nUso: ./programa --input ventas.csv --output resumen_ventas_usd.csv --threads 8");
        }
    }
    return args;
}
} // namespace

int main(int argc, char** argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    try {
        const double t_inicio_total = omp_get_wtime();
        const ArgumentosCli args = parsearArgumentos(argc, argv);
        omp_set_num_threads(args.num_hilos);

        ConfiguracionEntorno config(".env");
        const std::string url_base = config.obtener("CPYD_API_URL", "https://api.sebastian.cl/cpyd/v1");
        const std::string email = config.obtener("CPYD_EMAIL");
        const std::string api_key = config.obtener("CPYD_APIKEY");

        // 1. Lectura y validacion del archivo de transacciones
        const double t0_lectura = omp_get_wtime();
        ProcesadorVentas procesador;
        procesador.cargar_y_validar_csv(args.ruta_entrada, args.ruta_errores);
        const auto pares_unicos = procesador.obtener_pares_pais_fecha_unicos();
        const double t_lectura = omp_get_wtime() - t0_lectura;

        // 2. Autenticacion JWT unica al inicio y descarga paralela de tasas (con cache)
        const double t0_api = omp_get_wtime();
        ClienteHttp cliente_login;
        std::string token_jwt = cliente_login.autenticar(url_base, email, api_key);

        CacheTasas cache_tasas;
        if (!pares_unicos.empty()) {
            cache_tasas.descargar_tasas_paralelo(
                pares_unicos, url_base, email, api_key, token_jwt, args.num_hilos);
        }
        const double t_api = omp_get_wtime() - t0_api;

        // 3. Conversion a USD y acumulacion paralela sin condiciones de carrera
        const double t0_calculo = omp_get_wtime();
        procesador.acumular_ventas_usd_paralelo(cache_tasas, args.num_hilos);
        const double t_calculo = omp_get_wtime() - t0_calculo;

        // 4. Exportacion ordenada a CSV
        const double t0_escritura = omp_get_wtime();
        procesador.exportar_resumen_csv(args.ruta_salida);
        const double t_escritura = omp_get_wtime() - t0_escritura;

        const double t_total = omp_get_wtime() - t_inicio_total;

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "=== Resumen de Ejecucion (Hilos OpenMP: " << args.num_hilos << ") ===\n"
                  << "Ventas validas (2023-2025): " << procesador.total_ventas_validas() << "\n"
                  << "Registros invalidos/duplicados: " << procesador.total_registros_invalidos() << "\n"
                  << "Consultas unicas (pais, fecha): " << cache_tasas.total_tasas() << "\n"
                  << "Clientes agrupados en salida: " << procesador.total_clientes_agrupados() << "\n"
                  << "Tiempo lectura/validacion: " << t_lectura << " s\n"
                  << "Tiempo login + API tasas:  " << t_api << " s\n"
                  << "Tiempo conversion OpenMP:  " << t_calculo << " s\n"
                  << "Tiempo escritura CSV:      " << t_escritura << " s\n"
                  << "Tiempo TOTAL:              " << t_total << " s\n";

        curl_global_cleanup();
        return 0;
    } catch (const ErrorHttp& e) {
        std::cerr << "[Error HTTP " << e.codigo_http() << "]: " << e.what() << "\n";
        curl_global_cleanup();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[Error]: " << e.what() << "\n";
        curl_global_cleanup();
        return 1;
    }
}

