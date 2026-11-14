#include "venta.h"
#include "utem.h"

#include <cmath>
#include <regex>
#include <stdexcept>
#include <vector>

namespace {
bool esBisiesto(int anio) noexcept {
    return (anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0);
}

bool esFechaCalendarioValida(int anio, int mes, int dia) noexcept {
    if (anio < 1900 || anio > 2100 || mes < 1 || mes > 12 || dia < 1) {
        return false;
    }
    int dias_mes[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (esBisiesto(anio)) {
        dias_mes[1] = 29;
    }
    return dia <= dias_mes[mes - 1];
}
} // namespace

Venta::Venta(int pais, std::string id_cliente, std::string id_boleta, std::string fecha, double monto_local) {
    validar_pais(pais);
    validar_uuid(id_cliente, "id_cliente");
    validar_uuid(id_boleta, "id_boleta");
    fecha_ = extraer_y_validar_fecha(fecha);
    validar_monto(monto_local);

    pais_ = pais;
    id_cliente_ = std::move(id_cliente);
    id_boleta_ = std::move(id_boleta);
    monto_local_ = monto_local;
}

int Venta::obtener_pais() const noexcept {
    return pais_;
}

const std::string& Venta::obtener_id_cliente() const noexcept {
    return id_cliente_;
}

const std::string& Venta::obtener_id_boleta() const noexcept {
    return id_boleta_;
}

const std::string& Venta::obtener_fecha() const noexcept {
    return fecha_;
}

double Venta::obtener_monto_local() const noexcept {
    return monto_local_;
}

void Venta::validar_pais(int pais) {
    if (pais < 0 || pais > 2) {
        throw std::invalid_argument("Pais invalido (debe ser 0, 1 o 2): " + std::to_string(pais));
    }
}

void Venta::validar_uuid(const std::string& uuid, const std::string& nombre_campo) {
    static const std::regex patron_uuid(
        R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
    if (!std::regex_match(uuid, patron_uuid)) {
        throw std::invalid_argument("UUID invalido en campo '" + nombre_campo + "': " + uuid);
    }
}

std::string Venta::extraer_y_validar_fecha(const std::string& fecha_hora) {
    // Soporta YYYY-MM-DD o YYYY-MM-DD HH:MM:SS o YYYY-MM-DDTHH:MM:SS
    static const std::regex patron_iso(R"(^(\d{4})-(\d{2})-(\d{2})([ T]\d{2}:\d{2}(:\d{2}(\.\d+)?)?(Z|[+-]\d{2}:?\d{2})?)?$)");
    std::smatch coincidencia;
    if (std::regex_match(fecha_hora, coincidencia, patron_iso)) {
        const int anio = std::stoi(coincidencia[1].str());
        const int mes = std::stoi(coincidencia[2].str());
        const int dia = std::stoi(coincidencia[3].str());
        if (!esFechaCalendarioValida(anio, mes, dia)) {
            throw std::invalid_argument("Fecha calendario inexistente: " + fecha_hora);
        }
        return coincidencia[1].str() + "-" + coincidencia[2].str() + "-" + coincidencia[3].str();
    }

    throw std::invalid_argument("Formato de fecha/hora invalido: " + fecha_hora);
}

void Venta::validar_monto(double monto) {
    if (!std::isfinite(monto)) {
        throw std::invalid_argument("Monto de venta invalido: debe ser un numero finito");
    }
    if (monto <= 0.0) {
        throw std::invalid_argument("Monto de venta debe ser mayor que cero: " + std::to_string(monto));
    }
}

bool Venta::esta_en_rango_2023_2025(const std::string& fecha) noexcept {
    return (fecha >= "2023-01-01" && fecha <= "2025-12-31");
}

Venta Venta::desde_linea_csv(const std::string& linea, char separador) {
    const std::vector<std::string> campos = utem::dividirLineaCsv(linea, separador);
    if (campos.size() != 5) {
        throw std::invalid_argument("Cantidad de columnas invalida (se esperaban 5, se recibieron " + std::to_string(campos.size()) + ")");
    }

    std::size_t pos_entero = 0;
    const int pais = std::stoi(campos[0], &pos_entero);
    if (pos_entero != campos[0].size()) {
        throw std::invalid_argument("El campo pais no es un entero valido: " + campos[0]);
    }

    std::size_t pos_monto = 0;
    const double monto = std::stod(campos[4], &pos_monto);
    if (pos_monto != campos[4].size()) {
        throw std::invalid_argument("El campo monto no es un decimal valido: " + campos[4]);
    }

    return Venta(pais, campos[1], campos[2], campos[3], monto);
}

