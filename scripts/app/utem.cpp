#include "utem.h"
#include "error.h"
#include <sstream>

namespace utem {
std::string eliminarEspacios(const std::string& texto) {
    constexpr const char* BLANCOS = " \t\n\r";
    const std::size_t inicio = texto.find_first_not_of(BLANCOS);
    if (inicio == std::string::npos) {
        return {};
    }
    const std::size_t fin = texto.find_last_not_of(BLANCOS);
    return texto.substr(inicio, fin - inicio + 1);
}

std::string eliminarComillas(const std::string& texto) {
    if (texto.size() >= 2) {
        const char primer_caracter = texto.front();
        const char ultimo_caracter = texto.back();

        if ((primer_caracter == '"' && ultimo_caracter == '"')
            || (primer_caracter == '\'' && ultimo_caracter == '\'')) {
            return texto.substr(1, texto.size() - 2);
        }
    }
    return texto;
}

std::string mapearCodigoPais(int codigo_pais) {
    switch (codigo_pais) {
    case 0:
        return "chile";
    case 1:
        return "colombia";
    case 2:
        return "ecuador";
    default:
        throw Error("Codigo de pais invalido (se esperaba 0=Chile, 1=Colombia, 2=Ecuador): " + std::to_string(codigo_pais));
    }
}

std::vector<std::string> dividirLineaCsv(const std::string& linea, char separador) {
    std::vector<std::string> campos;
    std::stringstream ss(linea);
    std::string item;
    while (std::getline(ss, item, separador)) {
        campos.push_back(eliminarComillas(eliminarEspacios(item)));
    }
    return campos;
}
} // namespace utem

