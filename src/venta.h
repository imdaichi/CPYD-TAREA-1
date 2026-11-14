#ifndef APP_VENTA_H
#define APP_VENTA_H

#include <cstdint>
#include <string>

struct Uuid128 {
    std::uint64_t hi{ 0 };
    std::uint64_t lo{ 0 };

    bool operator==(const Uuid128& otro) const noexcept {
        return hi == otro.hi && lo == otro.lo;
    }
};

struct HashUuid128 {
    std::size_t operator()(const Uuid128& u) const noexcept {
        return static_cast<std::size_t>(u.hi ^ (u.lo * 0x9e3779b97f4a7c15ULL));
    }
};

class Venta {
public:
    explicit Venta(int pais, std::string id_cliente, std::string id_boleta, std::string fecha, double monto_local);
    ~Venta() = default;

    [[nodiscard]] int obtener_pais() const noexcept;
    [[nodiscard]] const std::string& obtener_id_cliente() const noexcept;
    [[nodiscard]] Uuid128 obtener_uuid_cliente() const noexcept;
    [[nodiscard]] const std::string& obtener_id_boleta() const noexcept;
    [[nodiscard]] Uuid128 obtener_uuid_boleta() const noexcept;
    [[nodiscard]] const std::string& obtener_fecha() const noexcept;
    [[nodiscard]] double obtener_monto_local() const noexcept;

    [[nodiscard]] static Venta desde_linea_csv(const std::string& linea, char separador = ',');
    [[nodiscard]] static bool esta_en_rango_2023_2025(const std::string& fecha) noexcept;
    [[nodiscard]] static Uuid128 parsear_y_validar_uuid(const std::string& uuid, const std::string& nombre_campo);

private:
    static void validar_pais(int pais);
    static std::string extraer_y_validar_fecha(const std::string& fecha_hora);
    static void validar_monto(double monto);

    int pais_{ 0 };
    std::string id_cliente_;
    std::string id_boleta_;
    Uuid128 uuid_cliente_{};
    Uuid128 uuid_boleta_{};
    std::string fecha_;
    double monto_local_{ 0.0 };
};

#endif // APP_VENTA_H
