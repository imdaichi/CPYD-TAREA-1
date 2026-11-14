#ifndef APP_TASA_CAMBIO_H
#define APP_TASA_CAMBIO_H

#include <array>
#include <json.hpp>
#include <map>
#include <set>
#include <string>
#include <utility>

class TasaCambio {
public:
    TasaCambio() = default;
    explicit TasaCambio(std::string pais, std::string fecha, double tasa);
    ~TasaCambio() = default;

    [[nodiscard]] const std::string& obtener_pais() const noexcept;
    [[nodiscard]] const std::string& obtener_fecha() const noexcept;
    [[nodiscard]] double obtener_tasa() const noexcept;

    void establecer_pais(const std::string& nuevo_pais);
    void establecer_fecha(const std::string& nueva_fecha);
    void establecer_tasa(double nueva_tasa);

private:
    static void validar_formato_fecha(const std::string& fecha);
    static void validar_tasa(double tasa);

    std::string pais_;
    std::string fecha_;
    double tasa_{ 0.0 };
};

void from_json(const nlohmann::json& j, TasaCambio& tasa);
TasaCambio crear_tasa_desde_json(const nlohmann::json& j);

class CacheTasas {
public:
    static constexpr std::size_t TOTAL_PAISES = 3;
    static constexpr std::size_t DIAS_PERIODO_2023_2025 = 1096; // 365 (2023) + 366 (2024) + 365 (2025)

    CacheTasas() = default;
    ~CacheTasas() = default;

    // Descarga todas las tasas del rango [fecha_inicio, fecha_fin] realizando
    // exactamente 3 peticiones HTTP GET (una por pais: chile, colombia, ecuador)
    // y renueva el token JWT si la API responde HTTP 403 (o 401).
    void descargar_tasas_por_rango(
        const std::string& url_base,
        const std::string& email,
        const std::string& api_key,
        std::string& token_jwt,
        int num_hilos,
        const std::string& fecha_inicio = "2023-01-01",
        const std::string& fecha_fin = "2025-12-31");

    [[nodiscard]] double obtener_tasa(int codigo_pais, const std::string& fecha) const;
    [[nodiscard]] std::size_t total_tasas() const noexcept;
    [[nodiscard]] std::size_t total_peticiones_http() const noexcept;
    [[nodiscard]] std::size_t total_fechas_respaldo() const noexcept;

private:
    [[nodiscard]] static int calcular_indice_dia_2023_2025(const std::string& fecha) noexcept;

    std::map<std::pair<int, std::string>, double> tasas_;
    std::array<std::array<double, DIAS_PERIODO_2023_2025>, TOTAL_PAISES> tabla_rapida_{ {} };
    std::size_t peticiones_http_{ 0 };
    std::size_t fechas_con_respaldo_{ 0 };
};

#endif // APP_TASA_CAMBIO_H
