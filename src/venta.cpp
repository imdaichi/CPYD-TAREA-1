#include "venta.h"
#include "utem.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
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

int valorHex(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

bool esDigito(char c) noexcept {
    return c >= '0' && c <= '9';
}

int leerDosDigitos(const std::string& s, std::size_t pos) noexcept {
    return (s[pos] - '0') * 10 + (s[pos + 1] - '0');
}
} // namespace

Venta::Venta(int pais, std::string id_cliente, std::string id_boleta, std::string fecha, double monto_local) {
    validar_pais(pais);
    uuid_cliente_ = parsear_y_validar_uuid(id_cliente, "id_cliente");
    uuid_boleta_ = parsear_y_validar_uuid(id_boleta, "id_boleta");
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

Uuid128 Venta::obtener_uuid_cliente() const noexcept {
    return uuid_cliente_;
}

const std::string& Venta::obtener_id_boleta() const noexcept {
    return id_boleta_;
}

Uuid128 Venta::obtener_uuid_boleta() const noexcept {
    return uuid_boleta_;
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

Uuid128 Venta::parsear_y_validar_uuid(const std::string& uuid, const std::string& nombre_campo) {
    // Formato estricto 8-4-4-4-12 (36 caracteres)
    if (uuid.size() != 36 || uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') {
        throw std::invalid_argument("UUID invalido en campo '" + nombre_campo + "': " + uuid);
    }

    std::uint64_t hi = 0;
    std::uint64_t lo = 0;
    int nibbles = 0;

    for (std::size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;
        }
        const int v = valorHex(uuid[i]);
        if (v < 0) {
            throw std::invalid_argument("UUID invalido en campo '" + nombre_campo + "': " + uuid);
        }
        if (nibbles < 16) {
            hi = (hi << 4) | static_cast<std::uint64_t>(v);
        } else {
            lo = (lo << 4) | static_cast<std::uint64_t>(v);
        }
        ++nibbles;
    }

    return Uuid128{ hi, lo };
}

std::string Venta::extraer_y_validar_fecha(const std::string& fecha_hora) {
    // Soporta YYYY-MM-DD, YYYY-MM-DD HH:MM, YYYY-MM-DD HH:MM:SS(.fff)(Z|+HH:MM) o con 'T'
    const std::size_t len = fecha_hora.size();
    if (len < 10 || fecha_hora[4] != '-' || fecha_hora[7] != '-') {
        throw std::invalid_argument("Formato de fecha/hora invalido: " + fecha_hora);
    }
    for (std::size_t i : { 0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u }) {
        if (!esDigito(fecha_hora[i])) {
            throw std::invalid_argument("Formato de fecha/hora invalido: " + fecha_hora);
        }
    }

    const int anio = (fecha_hora[0] - '0') * 1000 + (fecha_hora[1] - '0') * 100
        + (fecha_hora[2] - '0') * 10 + (fecha_hora[3] - '0');
    const int mes = leerDosDigitos(fecha_hora, 5);
    const int dia = leerDosDigitos(fecha_hora, 8);

    if (!esFechaCalendarioValida(anio, mes, dia)) {
        throw std::invalid_argument("Fecha calendario inexistente: " + fecha_hora);
    }

    if (len > 10) {
        if (len < 16 || (fecha_hora[10] != ' ' && fecha_hora[10] != 'T') || fecha_hora[13] != ':') {
            throw std::invalid_argument("Formato de fecha/hora invalido: " + fecha_hora);
        }
        if (!esDigito(fecha_hora[11]) || !esDigito(fecha_hora[12])
            || !esDigito(fecha_hora[14]) || !esDigito(fecha_hora[15])) {
            throw std::invalid_argument("Formato de fecha/hora invalido: " + fecha_hora);
        }
        const int hora = leerDosDigitos(fecha_hora, 11);
        const int minuto = leerDosDigitos(fecha_hora, 14);
        if (hora > 23 || minuto > 59) {
            throw std::invalid_argument("Hora fuera de rango en fecha/hora: " + fecha_hora);
        }

        if (len > 16) {
            if (len < 19 || fecha_hora[16] != ':' || !esDigito(fecha_hora[17]) || !esDigito(fecha_hora[18])) {
                throw std::invalid_argument("Formato de segundos invalido en fecha/hora: " + fecha_hora);
            }
            const int segundo = leerDosDigitos(fecha_hora, 17);
            if (segundo > 59) {
                throw std::invalid_argument("Segundos fuera de rango en fecha/hora: " + fecha_hora);
            }
        }
    }

    return fecha_hora.substr(0, 10);
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
        throw std::invalid_argument("Cantidad de columnas invalida (se esperaban 5, se recibieron "
            + std::to_string(campos.size()) + ")");
    }

    int pais = 0;
    const char* ini_pais = campos[0].data();
    const char* fin_pais = ini_pais + campos[0].size();
    const auto res_pais = std::from_chars(ini_pais, fin_pais, pais);
    if (res_pais.ec != std::errc() || res_pais.ptr != fin_pais) {
        throw std::invalid_argument("El campo pais no es un entero valido: " + campos[0]);
    }

    char* fin_monto = nullptr;
    const char* ini_monto = campos[4].c_str();
    const double monto = std::strtod(ini_monto, &fin_monto);
    if (fin_monto == ini_monto || *fin_monto != '\0') {
        throw std::invalid_argument("El campo monto no es un decimal valido: " + campos[4]);
    }

    return Venta(pais, campos[1], campos[2], campos[3], monto);
}
