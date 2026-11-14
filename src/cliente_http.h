#ifndef APP_CLIENTE_HTTP_H
#define APP_CLIENTE_HTTP_H

#include <string>

class ClienteHttp {
public:
    [[nodiscard]] std::string get(const std::string& url, const std::string& bearer_token = "") const;
    [[nodiscard]] std::string postJson(const std::string& url, const std::string& cuerpo_json) const;
    [[nodiscard]] std::string autenticar(const std::string& url_base, const std::string& email, const std::string& api_key) const;

private:
    static constexpr long timeout_conexion_segundos = 17L;
    static constexpr long timeout_total_segundos = 47L;
    static constexpr long maximo_redirecciones = 5L;
};

#endif // APP_CLIENTE_HTTP_H

