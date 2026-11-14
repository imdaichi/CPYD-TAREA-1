#ifndef APP_UTEM_H
#define APP_UTEM_H

#include <string>
#include <vector>

namespace utem {
[[nodiscard]] std::string eliminarEspacios(const std::string& texto);
[[nodiscard]] std::string eliminarComillas(const std::string& texto);
[[nodiscard]] std::string mapearCodigoPais(int codigo_pais);
[[nodiscard]] std::vector<std::string> dividirLineaCsv(const std::string& linea, char separador = ',');
} // namespace utem

#endif // APP_UTEM_H

