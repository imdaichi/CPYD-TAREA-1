#ifndef APP_PROCESADOR_VENTAS_H
#define APP_PROCESADOR_VENTAS_H

#include "tasa_cambio.h"
#include "venta.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class ProcesadorVentas {
public:
    ProcesadorVentas() = default;
    ~ProcesadorVentas() = default;

    void cargar_y_validar_csv(const std::string& ruta_entrada, const std::string& ruta_errores);
    [[nodiscard]] std::set<std::pair<int, std::string>> obtener_pares_pais_fecha_unicos() const;
    void acumular_ventas_usd_paralelo(const CacheTasas& cache_tasas, int num_hilos);
    void exportar_resumen_csv(const std::string& ruta_salida) const;

    [[nodiscard]] std::size_t total_ventas_validas() const noexcept;
    [[nodiscard]] std::size_t total_registros_invalidos() const noexcept;
    [[nodiscard]] std::size_t total_clientes_agrupados() const noexcept;

private:
    std::vector<Venta> ventas_validas_;
    std::map<std::pair<int, std::string>, double> totales_por_cliente_;
    std::size_t registros_invalidos_{ 0 };
};

#endif // APP_PROCESADOR_VENTAS_H

