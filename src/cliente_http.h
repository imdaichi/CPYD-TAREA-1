#ifndef APP_CLIENTE_HTTP_H
#define APP_CLIENTE_HTTP_H

#include <curl/curl.h>
#include <memory>
#include <string>

class ClienteHttp {
public:
    ClienteHttp();
    ~ClienteHttp() = default;

    ClienteHttp(const ClienteHttp&) = delete;
    ClienteHttp& operator=(const ClienteHttp&) = delete;
    ClienteHttp(ClienteHttp&&) noexcept = default;
    ClienteHttp& operator=(ClienteHttp&&) noexcept = default;

    [[nodiscard]] std::string get(const std::string& url, const std::string& bearer_token = "") const;
    [[nodiscard]] std::string postJson(const std::string& url, const std::string& cuerpo_json) const;
    [[nodiscard]] std::string autenticar(const std::string& url_base, const std::string& email, const std::string& api_key) const;

private:
    struct LiberadorCurl {
        void operator()(CURL* manejador) const noexcept;
    };

    std::unique_ptr<CURL, LiberadorCurl> manejador_;

    static constexpr long timeout_conexion_segundos = 17L;
    static constexpr long timeout_total_segundos = 47L;
    static constexpr long maximo_redirecciones = 5L;
};

#endif // APP_CLIENTE_HTTP_H

