#ifndef APP_PROCESADOR_VENTAS_H
#define APP_PROCESADOR_VENTAS_H

#include "tasa_cambio.h"
#include "venta.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>

class ProcesadorVentas {
public:
    ProcesadorVentas() = default;
    ~ProcesadorVentas() = default;

    // Lee el archivo CSV en paralelo particionando por rangos de bytes por hilo
    // (sin cargar el archivo completo en memoria), valida cada registro, filtra
    // el periodo 2023-2025, convierte a USD y acumula por (pais, id_cliente).
    void procesar_archivo_paralelo(
        const std::string& ruta_entrada,
        const std::string& ruta_errores,
        const CacheTasas& cache_tasas,
        int num_hilos);

    void exportar_resumen_csv(const std::string& ruta_salida) const;

    [[nodiscard]] std::size_t total_ventas_validas() const noexcept;
    [[nodiscard]] std::size_t total_registros_invalidos() const noexcept;
    [[nodiscard]] std::size_t total_fuera_de_rango() const noexcept;
    [[nodiscard]] std::size_t total_clientes_agrupados() const noexcept;

private:
    std::map<std::pair<int, std::string>, double> totales_por_cliente_;
    std::size_t ventas_validas_count_{ 0 };
    std::size_t registros_invalidos_{ 0 };
    std::size_t registros_fuera_rango_{ 0 };
};

#endif // APP_PROCESADOR_VENTAS_H
