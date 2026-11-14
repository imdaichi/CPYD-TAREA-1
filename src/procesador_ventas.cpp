#include "procesador_ventas.h"
#include "error.h"
#include "utem.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <omp.h>
#include <unordered_set>
#include <vector>

namespace {
constexpr std::size_t NUM_SHARDS_BOLETAS = 256;
constexpr std::size_t MAX_ERRORES_EN_BUFFER_POR_HILO = 50000;

struct ShardBoletas {
    omp_lock_t lock{};
    std::unordered_set<Uuid128, HashUuid128> boletas;
};

struct IncidenciaCsv {
    std::size_t linea_relativa{ 0 };
    std::string motivo;
    std::string contenido;
};

bool esEncabezadoCsv(const std::string& linea_limpia) noexcept {
    if (linea_limpia.empty()) {
        return false;
    }
    const char primero = linea_limpia[0];
    return !std::isdigit(static_cast<unsigned char>(primero)) && primero != '-';
}
} // namespace

void ProcesadorVentas::procesar_archivo_paralelo(
    const std::string& ruta_entrada,
    const std::string& ruta_errores,
    const CacheTasas& cache_tasas,
    int num_hilos) {

    std::ifstream probe(ruta_entrada, std::ios::binary | std::ios::ate);
    if (!probe.is_open()) {
        throw Error("No se pudo abrir el archivo de transacciones de entrada: " + ruta_entrada);
    }
    const auto pos_fin = probe.tellg();
    if (pos_fin < 0) {
        throw Error("No se pudo determinar el tamano del archivo de entrada: " + ruta_entrada);
    }
    const std::uint64_t tam_archivo = static_cast<std::uint64_t>(pos_fin);
    probe.close();

    std::ofstream archivo_errores(ruta_errores, std::ios::out | std::ios::trunc);
    if (!archivo_errores.is_open()) {
        throw Error("No se pudo abrir el archivo de registro de errores: " + ruta_errores);
    }
    archivo_errores << "linea,motivo,contenido\n";

    totales_por_cliente_.clear();
    ventas_validas_count_ = 0;
    registros_invalidos_ = 0;
    registros_fuera_rango_ = 0;

    if (tam_archivo == 0) {
        archivo_errores.flush();
        return;
    }

    std::array<ShardBoletas, NUM_SHARDS_BOLETAS> shards;
    for (auto& shard : shards) {
        omp_init_lock(&shard.lock);
    }

    const int hilos = (num_hilos > 0) ? num_hilos : 1;
    std::vector<std::size_t> lineas_por_hilo(static_cast<std::size_t>(hilos), 0);
    std::vector<std::vector<IncidenciaCsv>> errores_por_hilo(static_cast<std::size_t>(hilos));
    std::string error_lectura_hilo;

    #pragma omp parallel num_threads(hilos)
    {
        const int id_hilo = omp_get_thread_num();
        const int total_hilos = omp_get_num_threads();

        const std::uint64_t inicio_nominal = (tam_archivo * static_cast<std::uint64_t>(id_hilo))
            / static_cast<std::uint64_t>(total_hilos);
        const std::uint64_t fin_nominal = (tam_archivo * static_cast<std::uint64_t>(id_hilo + 1))
            / static_cast<std::uint64_t>(total_hilos);

        std::ifstream archivo_hilo(ruta_entrada, std::ios::binary);
        std::vector<char> buffer_io(1024 * 1024);
        archivo_hilo.rdbuf()->pubsetbuf(buffer_io.data(), static_cast<std::streamsize>(buffer_io.size()));

        if (!archivo_hilo.is_open()) {
            #pragma omp critical(error_critico)
            {
                if (error_lectura_hilo.empty()) {
                    error_lectura_hilo = "No se pudo abrir el archivo en el hilo " + std::to_string(id_hilo);
                }
            }
        } else if (inicio_nominal < fin_nominal) {
            std::uint64_t pos_actual = 0;

            if (id_hilo > 0 && inicio_nominal > 0) {
                archivo_hilo.seekg(static_cast<std::streamoff>(inicio_nominal - 1), std::ios::beg);
                std::string descarte;
                if (std::getline(archivo_hilo, descarte, '\n')) {
                    pos_actual = inicio_nominal + static_cast<std::uint64_t>(descarte.size());
                } else {
                    pos_actual = fin_nominal;
                }
            }

            std::map<std::pair<int, std::string>, double> acumulador_local;
            std::size_t validas_local = 0;
            std::size_t invalidas_local = 0;
            std::size_t fuera_rango_local = 0;
            std::size_t contador_lineas = 0;
            auto& buffer_errores = errores_por_hilo[static_cast<std::size_t>(id_hilo)];

            std::string linea;
            while (pos_actual < fin_nominal && std::getline(archivo_hilo, linea, '\n')) {
                pos_actual += static_cast<std::uint64_t>(linea.size()) + 1ULL;
                ++contador_lineas;

                const std::string linea_limpia = utem::eliminarEspacios(linea);
                if (linea_limpia.empty()) {
                    continue;
                }
                if (id_hilo == 0 && contador_lineas == 1 && esEncabezadoCsv(linea_limpia)) {
                    continue;
                }

                const char separador = (linea_limpia.find(';') != std::string::npos) ? ';' : ',';

                try {
                    const Venta venta = Venta::desde_linea_csv(linea_limpia, separador);
                    const Uuid128 uuid_boleta = venta.obtener_uuid_boleta();
                    const std::size_t idx_shard = static_cast<std::size_t>(uuid_boleta.hi) & (NUM_SHARDS_BOLETAS - 1);

                    bool es_duplicada = false;
                    omp_set_lock(&shards[idx_shard].lock);
                    const auto resultado_ins = shards[idx_shard].boletas.insert(uuid_boleta);
                    es_duplicada = !resultado_ins.second;
                    omp_unset_lock(&shards[idx_shard].lock);

                    if (es_duplicada) {
                        ++invalidas_local;
                        const std::string motivo = "Boleta duplicada: " + venta.obtener_id_boleta();
                        if (buffer_errores.size() < MAX_ERRORES_EN_BUFFER_POR_HILO) {
                            buffer_errores.push_back(IncidenciaCsv{ contador_lineas, motivo, linea_limpia });
                        } else {
                            #pragma omp critical(escritura_errores)
                            {
                                archivo_errores << contador_lineas << ",\"" << motivo << "\",\"" << linea_limpia << "\"\n";
                            }
                        }
                        continue;
                    }

                    if (!Venta::esta_en_rango_2023_2025(venta.obtener_fecha())) {
                        ++fuera_rango_local;
                        continue;
                    }

                    const double rate = cache_tasas.obtener_tasa(venta.obtener_pais(), venta.obtener_fecha());
                    const double monto_usd = venta.obtener_monto_local() / rate;
                    acumulador_local[{ venta.obtener_pais(), venta.obtener_id_cliente() }] += monto_usd;
                    ++validas_local;
                } catch (const std::exception& e) {
                    ++invalidas_local;
                    if (buffer_errores.size() < MAX_ERRORES_EN_BUFFER_POR_HILO) {
                        buffer_errores.push_back(IncidenciaCsv{ contador_lineas, e.what(), linea_limpia });
                    } else {
                        #pragma omp critical(escritura_errores)
                        {
                            archivo_errores << contador_lineas << ",\"" << e.what() << "\",\"" << linea_limpia << "\"\n";
                        }
                    }
                }
            }

            lineas_por_hilo[static_cast<std::size_t>(id_hilo)] = contador_lineas;

            #pragma omp critical(mezcla_totales)
            {
                for (const auto& par : acumulador_local) {
                    totales_por_cliente_[par.first] += par.second;
                }
                ventas_validas_count_ += validas_local;
                registros_invalidos_ += invalidas_local;
                registros_fuera_rango_ += fuera_rango_local;
            }
        }
    }

    for (auto& shard : shards) {
        omp_destroy_lock(&shard.lock);
    }

    if (!error_lectura_hilo.empty()) {
        throw Error(error_lectura_hilo);
    }

    // Volcar errores con su numero de linea global exacto en orden
    std::size_t offset_linea = 0;
    for (int h = 0; h < hilos; ++h) {
        const auto idx = static_cast<std::size_t>(h);
        for (const auto& inc : errores_por_hilo[idx]) {
            archivo_errores << (offset_linea + inc.linea_relativa)
                            << ",\"" << inc.motivo << "\",\"" << inc.contenido << "\"\n";
        }
        offset_linea += lineas_por_hilo[idx];
    }

    if (!archivo_errores.flush() || archivo_errores.fail()) {
        throw Error("Error de E/S al vaciar el buffer del archivo de errores: " + ruta_errores);
    }
}

void ProcesadorVentas::exportar_resumen_csv(const std::string& ruta_salida) const {
    std::ofstream archivo_csv(ruta_salida, std::ios::out | std::ios::trunc);
    if (!archivo_csv.is_open()) {
        throw Error("No se pudo abrir el archivo de salida para escritura: " + ruta_salida);
    }

    archivo_csv << "pais,id_cliente,total_usd\n";
    archivo_csv << std::fixed << std::setprecision(2);

    for (const auto& item : totales_por_cliente_) {
        const int pais = item.first.first;
        const std::string& id_cliente = item.first.second;
        const double total_usd = item.second;
        archivo_csv << pais << "," << id_cliente << "," << total_usd << "\n";
    }

    if (!archivo_csv.flush() || archivo_csv.fail()) {
        throw Error("Error de E/S al escribir o vaciar el buffer del archivo CSV: " + ruta_salida);
    }
}

std::size_t ProcesadorVentas::total_ventas_validas() const noexcept {
    return ventas_validas_count_;
}

std::size_t ProcesadorVentas::total_registros_invalidos() const noexcept {
    return registros_invalidos_;
}

std::size_t ProcesadorVentas::total_fuera_de_rango() const noexcept {
    return registros_fuera_rango_;
}

std::size_t ProcesadorVentas::total_clientes_agrupados() const noexcept {
    return totales_por_cliente_.size();
}
