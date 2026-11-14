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
#include <vector>

namespace {
struct ArgumentosCli {
    std::string ruta_entrada = "ventas.csv";
    std::string ruta_salida = "resumen_ventas_usd.csv";
    std::string ruta_errores = "errores.csv";
    int num_hilos = omp_get_max_threads();
    bool modo_benchmark = false;
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
        } else if (arg == "--benchmark") {
            args.modo_benchmark = true;
        } else {
            throw Error("Argumento desconocido o incompleto: " + arg
                + "\nUso: ./programa --input ventas.csv --output resumen_ventas_usd.csv --threads 8 [--benchmark]");
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

        // 1. Autenticacion JWT unica al inicio y consulta de tasas por rango (3 peticiones: chile, colombia, ecuador)
        const double t0_api = omp_get_wtime();
        ClienteHttp cliente_login;
        std::string token_jwt = cliente_login.autenticar(url_base, email, api_key);

        CacheTasas cache_tasas;
        cache_tasas.descargar_tasas_por_rango(url_base, email, api_key, token_jwt, args.num_hilos);
        const double t_api = omp_get_wtime() - t0_api;

        if (args.modo_benchmark) {
            const std::vector<int> hilos_prueba = { 1, 2, 4, 8 };
            double tiempo_base_1_hilo = 0.0;

            std::cout << std::fixed << std::setprecision(4);
            std::cout << "=== Benchmark OpenMP (1, 2, 4 y 8 hilos) ===\n"
                      << "Tiempo autenticacion + 3 consultas de rango API: " << t_api << " s\n\n"
                      << "Hilos | T. Procesamiento (s) | T. Total (s) | Speedup S(p) | Eficiencia E(p)\n"
                      << "------+----------------------+--------------+--------------+----------------\n";

            for (int p : hilos_prueba) {
                omp_set_num_threads(p);
                ProcesadorVentas proc;

                const double t0_proc = omp_get_wtime();
                proc.procesar_archivo_paralelo(args.ruta_entrada, args.ruta_errores, cache_tasas, p);
                const double t_proc = omp_get_wtime() - t0_proc;

                const double t0_esc = omp_get_wtime();
                proc.exportar_resumen_csv(args.ruta_salida);
                const double t_esc = omp_get_wtime() - t0_esc;

                const double t_corrida = t_proc + t_esc;
                if (p == 1) {
                    tiempo_base_1_hilo = t_corrida;
                }
                const double speedup = (t_corrida > 0.0) ? (tiempo_base_1_hilo / t_corrida) : 1.0;
                const double eficiencia = speedup / static_cast<double>(p);

                std::cout << std::setw(5) << p << " | "
                          << std::setw(20) << t_proc << " | "
                          << std::setw(12) << t_corrida << " | "
                          << std::setw(12) << speedup << " | "
                          << std::setw(14) << (eficiencia * 100.0) << "%\n";
            }

            curl_global_cleanup();
            return 0;
        }

        // 2. Lectura en paralelo por particiones (sin cargar archivo completo en memoria),
        //    validacion, filtrado 2023-2025, conversion a USD y acumulacion por hilo
        const double t0_procesamiento = omp_get_wtime();
        ProcesadorVentas procesador;
        procesador.procesar_archivo_paralelo(args.ruta_entrada, args.ruta_errores, cache_tasas, args.num_hilos);
        const double t_procesamiento = omp_get_wtime() - t0_procesamiento;

        // 3. Exportacion ordenada a CSV
        const double t0_escritura = omp_get_wtime();
        procesador.exportar_resumen_csv(args.ruta_salida);
        const double t_escritura = omp_get_wtime() - t0_escritura;

        const double t_total = omp_get_wtime() - t_inicio_total;

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "=== Resumen de Ejecucion (Hilos OpenMP: " << args.num_hilos << ") ===\n"
                  << "Peticiones HTTP de rango API:   " << cache_tasas.total_peticiones_http() << "\n"
                  << "Tasas en cache (2023-2025):     " << cache_tasas.total_tasas()
                  << " (respaldo fines de semana/feriados: " << cache_tasas.total_fechas_respaldo() << ")\n"
                  << "Ventas validas (2023-2025):     " << procesador.total_ventas_validas() << "\n"
                  << "Ventas fuera de rango:          " << procesador.total_fuera_de_rango() << "\n"
                  << "Registros invalidos/duplicados: " << procesador.total_registros_invalidos() << "\n"
                  << "Clientes agrupados en salida:   " << procesador.total_clientes_agrupados() << "\n"
                  << "Tiempo login + API (3 rangos):  " << t_api << " s\n"
                  << "Tiempo lectura/conv. OpenMP:    " << t_procesamiento << " s\n"
                  << "Tiempo escritura CSV:           " << t_escritura << " s\n"
                  << "Tiempo TOTAL:                   " << t_total << " s\n";

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
