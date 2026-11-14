#ifndef APP_CONFIGURACION_ENTORNO_H
#define APP_CONFIGURACION_ENTORNO_H

#include "error.h"
#include <string>
#include <unordered_map>

class ConfiguracionEntorno {
public:
    explicit ConfiguracionEntorno(const std::string& ruta_archivo = ".env");
    ~ConfiguracionEntorno() = default;

    [[nodiscard]] std::string obtener(const std::string& clave) const;
    [[nodiscard]] std::string obtener(const std::string& clave, const std::string& valor_por_defecto) const;
    [[nodiscard]] bool contiene(const std::string& clave) const;

private:
    void cargarArchivo(const std::string& ruta_archivo);
    std::unordered_map<std::string, std::string> variables_;
};

#endif // APP_CONFIGURACION_ENTORNO_H

