#include "configuracion_entorno.h"
#include "error.h"
#include "utem.h"

#include <cstdlib>
#include <fstream>
#include <string>

namespace {
std::string mensajeErrorSintaxis(const std::string& descripcion, std::size_t numero_linea, const std::string& linea_contenido) {
    return "Error de sintaxis en el archivo de configuracion (linea "
        + std::to_string(numero_linea) + "): " + descripcion + " en '" + linea_contenido + "'";
}
} // namespace

ConfiguracionEntorno::ConfiguracionEntorno(const std::string& ruta_archivo) {
    cargarArchivo(ruta_archivo);
}

void ConfiguracionEntorno::cargarArchivo(const std::string& ruta_archivo) {
    std::ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        // Si no está en el directorio actual, probamos ../../.env por si se ejecuta desde scripts/app
        archivo.open("../../" + ruta_archivo);
        if (!archivo.is_open()) {
            return;
        }
    }
    std::string linea;
    std::size_t numero_linea = 0;
    while (std::getline(archivo, linea)) {
        ++numero_linea;
        const std::string linea_limpia = utem::eliminarEspacios(linea);
        if (linea_limpia.empty() || linea_limpia[0] == '#') {
            continue;
        }
        const std::size_t posicion_igual = linea_limpia.find('=');
        if (posicion_igual == std::string::npos) {
            throw Error(mensajeErrorSintaxis("Falta el caracter '='", numero_linea, linea));
        }
        const std::string clave = utem::eliminarEspacios(linea_limpia.substr(0, posicion_igual));
        if (clave.empty()) {
            throw Error(mensajeErrorSintaxis("la clave no puede estar vacia", numero_linea, linea));
        }
        std::string valor = utem::eliminarEspacios(linea_limpia.substr(posicion_igual + 1));

        valor = utem::eliminarComillas(valor);
        variables_[clave] = valor;
    }
}

std::string ConfiguracionEntorno::obtener(const std::string& clave) const {
    const auto iterador = variables_.find(clave);
    if (iterador != variables_.end() && !iterador->second.empty()) {
        return iterador->second;
    }
    const char* env_sistema = std::getenv(clave.c_str());
    if (env_sistema != nullptr && env_sistema[0] != '\0') {
        return std::string(env_sistema);
    }
    throw Error("Variable de entorno no encontrada en la configuracion: '" + clave + "'");
}

std::string ConfiguracionEntorno::obtener(const std::string& clave, const std::string& valor_por_defecto) const {
    const auto iterador = variables_.find(clave);
    if (iterador != variables_.end() && !iterador->second.empty()) {
        return iterador->second;
    }
    const char* env_sistema = std::getenv(clave.c_str());
    if (env_sistema != nullptr && env_sistema[0] != '\0') {
        return std::string(env_sistema);
    }
    return valor_por_defecto;
}

bool ConfiguracionEntorno::contiene(const std::string& clave) const {
    const auto iterador = variables_.find(clave);
    if (iterador != variables_.end() && !iterador->second.empty()) {
        return true;
    }
    const char* env_sistema = std::getenv(clave.c_str());
    return (env_sistema != nullptr && env_sistema[0] != '\0');
}

