#include "tasa_cambio.h"
#include "cliente_http.h"
#include "error.h"
#include "error_http.h"
#include "utem.h"

#include <cmath>
#include <iostream>
#include <omp.h>
#include <regex>
#include <stdexcept>
#include <vector>

TasaCambio::TasaCambio(std::string pais, std::string fecha, double tasa) {
    establecer_pais(pais);
    establecer_fecha(fecha);
    establecer_tasa(tasa);
}

const std::string& TasaCambio::obtener_pais() const noexcept {
    return pais_;
}

const std::string& TasaCambio::obtener_fecha() const noexcept {
    return fecha_;
}

double TasaCambio::obtener_tasa() const noexcept {
    return tasa_;
}

void TasaCambio::establecer_pais(const std::string& nuevo_pais) {
    if (nuevo_pais.empty()) {
        throw std::invalid_argument("El nombre del pais en TasaCambio no puede estar vacio");
    }
    pais_ = nuevo_pais;
}

void TasaCambio::establecer_fecha(const std::string& nueva_fecha) {
    validar_formato_fecha(nueva_fecha);
    fecha_ = nueva_fecha;
}

void TasaCambio::establecer_tasa(const double nueva_tasa) {
    validar_tasa(nueva_tasa);
    tasa_ = nueva_tasa;
}

void TasaCambio::validar_formato_fecha(const std::string& fecha) {
    static const std::regex patron_fecha(R"(^\d{4}-\d{2}-\d{2}$)");
    if (!std::regex_match(fecha, patron_fecha)) {
        throw std::invalid_argument(
            "Formato de fecha invalido en TasaCambio. Se esperaba YYYY-MM-DD, se recibio: " + fecha);
    }
}

void TasaCambio::validar_tasa(const double tasa) {
    if (!std::isfinite(tasa)) {
        throw std::invalid_argument("Valor de tasa invalido: debe ser un numero finito");
    }
    if (tasa <= 0.0) {
        throw std::invalid_argument(
            "Valor de tasa invalido: debe ser mayor que cero, se recibio: " + std::to_string(tasa));
    }
}

void from_json(const nlohmann::json& j, TasaCambio& tasa) {
    tasa = crear_tasa_desde_json(j);
}

TasaCambio crear_tasa_desde_json(const nlohmann::json& j) {
    if (!j.contains("country") || !j.at("country").is_string()) {
        throw std::invalid_argument("JSON invalido: falta o no es cadena el campo 'country'");
    }
    if (!j.contains("date") || !j.at("date").is_string()) {
        throw std::invalid_argument("JSON invalido: falta o no es cadena el campo 'date'");
    }
    if (!j.contains("rate") || !j.at("rate").is_number()) {
        throw std::invalid_argument("JSON invalido: falta o no es numerico el campo 'rate'");
    }

    const std::string pais = j.at("country").get<std::string>();
    const std::string fecha = j.at("date").get<std::string>();
    const double valor_tasa = j.at("rate").get<double>();

    return TasaCambio(pais, fecha, valor_tasa);
}

void CacheTasas::descargar_tasas_paralelo(
    const std::set<std::pair<int, std::string>>& pares_unicos,
    const std::string& url_base,
    const std::string& email,
    const std::string& api_key,
    std::string& token_jwt,
    int num_hilos) {

    const std::vector<std::pair<int, std::string>> vector_pares(pares_unicos.begin(), pares_unicos.end());
    const std::size_t total = vector_pares.size();
    std::vector<double> resultados(total, -1.0);

    #pragma omp parallel num_threads(num_hilos)
    {
        ClienteHttp cliente_hilo;

        #pragma omp for schedule(dynamic)
        for (std::size_t i = 0; i < total; ++i) {
            const int codigo_pais = vector_pares[i].first;
            const std::string& fecha = vector_pares[i].second;
            const std::string nombre_pais = utem::mapearCodigoPais(codigo_pais);
            const std::string url = url_base + "/exchanges/" + nombre_pais + "/" + fecha;

            for (int intento = 0; intento < 3; ++intento) {
                std::string token_actual;
                #pragma omp critical(acceso_token)
                {
                    token_actual = token_jwt;
                }

                try {
                    const std::string cuerpo = cliente_hilo.get(url, token_actual);
                    const nlohmann::json j = nlohmann::json::parse(cuerpo);
                    const TasaCambio tc = crear_tasa_desde_json(j);
                    resultados[i] = tc.obtener_tasa();
                    break;
                } catch (const ErrorHttp& e) {
                    if (e.codigo_http() == 401) {
                        // Token expirado: renovar una sola vez de forma sincronizada
                        #pragma omp critical(acceso_token)
                        {
                            if (token_jwt == token_actual) {
                                try {
                                    token_jwt = cliente_hilo.autenticar(url_base, email, api_key);
                                } catch (...) {
                                }
                            }
                        }
                    } else if (e.codigo_http() == 404) {
                        // Fecha sin tasa disponible en la API
                        break;
                    }
                } catch (...) {
                    // Respuesta invalida o fallo de parseo
                }
            }
        }
    }

    // Cargar resultados y aplicar politica para fechas sin tasa disponible:
    // Se usa la ultima tasa disponible anterior del mismo pais (o la siguiente posterior si es la primera).
    tasas_.clear();
    for (std::size_t i = 0; i < total; ++i) {
        if (resultados[i] > 0.0) {
            tasas_[vector_pares[i]] = resultados[i];
        }
    }

    for (std::size_t i = 0; i < total; ++i) {
        if (resultados[i] <= 0.0) {
            const int pais = vector_pares[i].first;
            const std::string& fecha = vector_pares[i].second;
            double tasa_respaldo = -1.0;

            auto it = tasas_.lower_bound({ pais, fecha });
            if (it != tasas_.begin()) {
                auto prev = std::prev(it);
                if (prev->first.first == pais) {
                    tasa_respaldo = prev->second;
                }
            }
            if (tasa_respaldo <= 0.0 && it != tasas_.end() && it->first.first == pais) {
                tasa_respaldo = it->second;
            }

            if (tasa_respaldo > 0.0) {
                tasas_[{ pais, fecha }] = tasa_respaldo;
                std::cerr << "[AVISO] Sin tasa en API para pais=" << pais << " fecha=" << fecha
                          << ". Se aplica ultima tasa disponible: " << tasa_respaldo << "\n";
            } else {
                throw Error("No existe ninguna tasa de cambio disponible para el pais " + std::to_string(pais));
            }
        }
    }
}

double CacheTasas::obtener_tasa(int codigo_pais, const std::string& fecha) const {
    const auto it = tasas_.find({ codigo_pais, fecha });
    if (it == tasas_.end()) {
        throw Error("Tasa no encontrada en cache para pais=" + std::to_string(codigo_pais) + " fecha=" + fecha);
    }
    return it->second;
}

std::size_t CacheTasas::total_tasas() const noexcept {
    return tasas_.size();
}

