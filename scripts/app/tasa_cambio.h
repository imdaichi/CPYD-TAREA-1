#ifndef APP_TASA_CAMBIO_H
#define APP_TASA_CAMBIO_H

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
    CacheTasas() = default;
    ~CacheTasas() = default;

    void descargar_tasas_paralelo(
        const std::set<std::pair<int, std::string>>& pares_unicos,
        const std::string& url_base,
        const std::string& email,
        const std::string& api_key,
        std::string& token_jwt,
        int num_hilos);

    [[nodiscard]] double obtener_tasa(int codigo_pais, const std::string& fecha) const;
    [[nodiscard]] std::size_t total_tasas() const noexcept;

private:
    std::map<std::pair<int, std::string>, double> tasas_;
};

#endif // APP_TASA_CAMBIO_H

