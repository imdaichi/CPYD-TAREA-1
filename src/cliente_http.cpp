#include "cliente_http.h"
#include "error_http.h"

#include <cstddef>
#include <curl/curl.h>
#include <curl/easy.h>
#include <json.hpp>
#include <memory>
#include <string>

namespace {
struct LiberadorHeaders {
    void operator()(curl_slist* lista) const noexcept {
        if (lista) {
            curl_slist_free_all(lista);
        }
    }
};

using ManejadorHeaders = std::unique_ptr<curl_slist, LiberadorHeaders>;

std::size_t acumularRespuesta(char* datos, std::size_t tam, std::size_t n, void* destino) {
    const std::size_t total = tam * n;
    try {
        auto* cadena_destino = static_cast<std::string*>(destino);
        cadena_destino->append(datos, total);
        return total;
    } catch (...) {
        return 0;
    }
}
} // namespace

        void ClienteHttp::LiberadorCurl::operator()(CURL* manejador) const noexcept {
            if (manejador) {
                curl_easy_cleanup(manejador);
            }
        }

        ClienteHttp::ClienteHttp()
            : manejador_(curl_easy_init()) {
            if (!manejador_) {
                throw ErrorHttp("Fallo critico: no se pudo inicializar el manejador de libcurl (curl_easy_init retorno nullptr)");
            }
        }

        std::string ClienteHttp::get(const std::string& url, const std::string& bearer_token) const {
            if (!manejador_) {
                throw ErrorHttp("Fallo critico: manejador de libcurl no inicializado");
            }

            std::string cuerpo_respuesta;
            char buffer_error[CURL_ERROR_SIZE] = { 0 };

            curl_slist* headers_raw = nullptr;
            headers_raw = curl_slist_append(headers_raw, "accept: application/json");
            if (!bearer_token.empty()) {
                const std::string header_auth = "Authorization: Bearer " + bearer_token;
                headers_raw = curl_slist_append(headers_raw, header_auth.c_str());
            }
            ManejadorHeaders headers(headers_raw);

            CURL* m = manejador_.get();
            // Reinicia opciones de la peticion anterior pero conserva conexiones vivas (Keep-Alive), cache DNS y sesion TLS
            curl_easy_reset(m);
            curl_easy_setopt(m, CURLOPT_URL, url.c_str());
            curl_easy_setopt(m, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(m, CURLOPT_HTTPHEADER, headers.get());
            curl_easy_setopt(m, CURLOPT_WRITEFUNCTION, &acumularRespuesta);
            curl_easy_setopt(m, CURLOPT_WRITEDATA, static_cast<void*>(&cuerpo_respuesta));
            curl_easy_setopt(m, CURLOPT_ERRORBUFFER, buffer_error);
            curl_easy_setopt(m, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(m, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(m, CURLOPT_CONNECTTIMEOUT, timeout_conexion_segundos);
            curl_easy_setopt(m, CURLOPT_TIMEOUT, timeout_total_segundos);
            curl_easy_setopt(m, CURLOPT_MAXREDIRS, maximo_redirecciones);
            curl_easy_setopt(m, CURLOPT_TCP_KEEPALIVE, 1L);

            const CURLcode resultado = curl_easy_perform(m);
            if (resultado != CURLE_OK) {
                std::string mensaje_detalle = (buffer_error[0] != '\0')
                    ? std::string(buffer_error)
                    : std::string(curl_easy_strerror(resultado));

                throw ErrorHttp("Error de transporte en libcurl: " + mensaje_detalle);
            }

            long codigo_http = 0;
            const CURLcode resultado_info = curl_easy_getinfo(m, CURLINFO_RESPONSE_CODE, &codigo_http);
            if (resultado_info != CURLE_OK) {
                throw ErrorHttp("Fallo al obtener el codigo de estado HTTP: " + std::string(curl_easy_strerror(resultado_info)));
            }

            if (codigo_http < 200 || codigo_http > 299) {
                throw ErrorHttp("Respuesta HTTP inesperada. Codigo de estado: " + std::to_string(codigo_http), codigo_http);
            }

            return cuerpo_respuesta;
        }

        std::string ClienteHttp::postJson(const std::string& url, const std::string& cuerpo_json) const {
                if (!manejador_) {
                    throw ErrorHttp("Fallo critico: manejador de libcurl no inicializado");
                }

                std::string cuerpo_respuesta;
                char buffer_error[CURL_ERROR_SIZE] = { 0 };

                curl_slist* headers_raw = nullptr;
                headers_raw = curl_slist_append(headers_raw, "Content-Type: application/json");
                headers_raw = curl_slist_append(headers_raw, "accept: application/json");
                ManejadorHeaders headers(headers_raw);

                CURL* m = manejador_.get();
                // Reinicia opciones de la peticion anterior pero conserva conexiones vivas (Keep-Alive), cache DNS y sesion TLS
                curl_easy_reset(m);
                curl_easy_setopt(m, CURLOPT_URL, url.c_str());
                curl_easy_setopt(m, CURLOPT_POST, 1L);
                curl_easy_setopt(m, CURLOPT_POSTFIELDS, cuerpo_json.c_str());
                curl_easy_setopt(m, CURLOPT_POSTFIELDSIZE, static_cast<long>(cuerpo_json.size()));
                curl_easy_setopt(m, CURLOPT_HTTPHEADER, headers.get());
                curl_easy_setopt(m, CURLOPT_WRITEFUNCTION, &acumularRespuesta);
                curl_easy_setopt(m, CURLOPT_WRITEDATA, static_cast<void*>(&cuerpo_respuesta));
                curl_easy_setopt(m, CURLOPT_ERRORBUFFER, buffer_error);
                curl_easy_setopt(m, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(m, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(m, CURLOPT_CONNECTTIMEOUT, timeout_conexion_segundos);
                curl_easy_setopt(m, CURLOPT_TIMEOUT, timeout_total_segundos);
                curl_easy_setopt(m, CURLOPT_MAXREDIRS, maximo_redirecciones);
                curl_easy_setopt(m, CURLOPT_TCP_KEEPALIVE, 1L);

                const CURLcode resultado = curl_easy_perform(m);
                if (resultado != CURLE_OK) {
                    std::string mensaje_detalle = (buffer_error[0] != '\0')
                        ? std::string(buffer_error)
                        : std::string(curl_easy_strerror(resultado));

                    throw ErrorHttp("Error de transporte en libcurl (POST): " + mensaje_detalle);
                }

                long codigo_http = 0;
                const CURLcode resultado_info = curl_easy_getinfo(m, CURLINFO_RESPONSE_CODE, &codigo_http);
                if (resultado_info != CURLE_OK) {
                    throw ErrorHttp("Fallo al obtener el codigo de estado HTTP: " + std::string(curl_easy_strerror(resultado_info)));
                }

                if (codigo_http < 200 || codigo_http > 299) {
                    throw ErrorHttp("Respuesta HTTP inesperada en POST. Codigo de estado: " + std::to_string(codigo_http), codigo_http);
                }

                return cuerpo_respuesta;
            }

            std::string ClienteHttp::autenticar(const std::string& url_base, const std::string& email, const std::string& api_key) const {
                const std::string endpoint = url_base + "/login/authenticate";
                const nlohmann::json payload = {
                    { "email", email },
                    { "apiKey", api_key }
                };

                const std::string respuesta_str = postJson(endpoint, payload.dump());
                const nlohmann::json respuesta_json = nlohmann::json::parse(respuesta_str);

                if (!respuesta_json.contains("jwt") || !respuesta_json.at("jwt").is_string()) {
                    throw ErrorHttp("La respuesta de autenticacion no contiene un token 'jwt' valido");
                }

                const std::string jwt = respuesta_json.at("jwt").get<std::string>();
                if (jwt.empty()) {
                    throw ErrorHttp("El token 'jwt' recibido esta vacio");
                }

                return jwt;
            }
