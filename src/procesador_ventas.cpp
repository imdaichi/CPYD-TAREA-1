#include "procesador_ventas.h"
#include "error.h"
#include "utem.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <omp.h>
#include <unordered_set>

namespace {
bool esEncabezadoCsv(const std::string& linea_limpia) {
    if (linea_limpia.empty()) {
        return false;
    }
    const char primero = linea_limpia[0];
    return !std::isdigit(static_cast<unsigned char>(primero)) && primero != '-';
}
} // namespace

void ProcesadorVentas::cargar_y_validar_csv(const std::string& ruta_entrada, const std::string& ruta_errores) {
    std::ifstream archivo_entrada(ruta_entrada);
    if (!archivo_entrada.is_open()) {
        throw Error("No se pudo abrir el archivo de transacciones de entrada: " + ruta_entrada);
    }

    std::ofstream archivo_errores(ruta_errores, std::ios::out | std::ios::trunc);
    if (!archivo_errores.is_open()) {
        throw Error("No se pudo abrir el archivo de registro de errores: " + ruta_errores);
    }
    archivo_errores << "linea,motivo,contenido\n";

    ventas_validas_.clear();
    registros_invalidos_ = 0;

    std::unordered_set<std::string> boletas_vistas;
    std::string linea;
    std::size_t numero_linea = 0;

    while (std::getline(archivo_entrada, linea)) {
        ++numero_linea;
        const std::string linea_limpia = utem::eliminarEspacios(linea);
        if (linea_limpia.empty()) {
            continue;
        }
        if (numero_linea == 1 && esEncabezadoCsv(linea_limpia)) {
            continue;
        }

        const char separador = (linea_limpia.find(';') != std::string::npos) ? ';' : ',';

        try {
            Venta venta = Venta::desde_linea_csv(linea_limpia, separador);

            if (boletas_vistas.find(venta.obtener_id_boleta()) != boletas_vistas.end()) {
                ++registros_invalidos_;
                archivo_errores << numero_linea << ",\"Boleta duplicada: " << venta.obtener_id_boleta()
                                << "\",\"" << linea_limpia << "\"\n";
                continue;
            }

            if (!Venta::esta_en_rango_2023_2025(venta.obtener_fecha())) {
                boletas_vistas.insert(venta.obtener_id_boleta());
                continue;
            }

            boletas_vistas.insert(venta.obtener_id_boleta());
            ventas_validas_.push_back(std::move(venta));
        } catch (const std::exception& e) {
            ++registros_invalidos_;
            archivo_errores << numero_linea << ",\"" << e.what() << "\",\"" << linea_limpia << "\"\n";
        }
    }

    if (!archivo_errores.flush() || archivo_errores.fail()) {
        throw Error("Error de E/S al vaciar el buffer del archivo de errores: " + ruta_errores);
    }
}

std::set<std::pair<int, std::string>> ProcesadorVentas::obtener_pares_pais_fecha_unicos() const {
    std::set<std::pair<int, std::string>> pares;
    for (const Venta& v : ventas_validas_) {
        pares.insert({ v.obtener_pais(), v.obtener_fecha() });
    }
    return pares;
}

void ProcesadorVentas::acumular_ventas_usd_paralelo(const CacheTasas& cache_tasas, int num_hilos) {
    totales_por_cliente_.clear();
    const std::size_t n = ventas_validas_.size();

    #pragma omp parallel num_threads(num_hilos)
    {
        std::map<std::pair<int, std::string>, double> acumulador_local;

        #pragma omp for schedule(static)
        for (std::size_t i = 0; i < n; ++i) {
            const Venta& v = ventas_validas_[i];
            const double rate = cache_tasas.obtener_tasa(v.obtener_pais(), v.obtener_fecha());
            const double monto_usd = v.obtener_monto_local() / rate;
            acumulador_local[{ v.obtener_pais(), v.obtener_id_cliente() }] += monto_usd;
        }

        #pragma omp critical(mezcla_totales)
        {
            for (const auto& par : acumulador_local) {
                totales_por_cliente_[par.first] += par.second;
            }
        }
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
    return ventas_validas_.size();
}

std::size_t ProcesadorVentas::total_registros_invalidos() const noexcept {
    return registros_invalidos_;
}

std::size_t ProcesadorVentas::total_clientes_agrupados() const noexcept {
    return totales_por_cliente_.size();
}

