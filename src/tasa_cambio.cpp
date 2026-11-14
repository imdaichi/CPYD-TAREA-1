#include "tasa_cambio.h"
#include "cliente_http.h"
#include "error.h"
#include "error_http.h"
#include "utem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <omp.h>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
bool esBisiesto(int anio) noexcept {
    return (anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0);
}

int diasEnMes(int anio, int mes) noexcept {
    static constexpr int dias[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (mes == 2 && esBisiesto(anio)) {
        return 29;
    }
    return dias[mes - 1];
}

std::vector<std::string> generarFechasPeriodo2023A2025() {
    std::vector<std::string> fechas;
    fechas.reserve(CacheTasas::DIAS_PERIODO_2023_2025);
    char buffer[32]; // holgura para que GCC no advierta truncamiento en snprintf
    for (int anio = 2023; anio <= 2025; ++anio) {
        for (int mes = 1; mes <= 12; ++mes) {
            const int max_dia = diasEnMes(anio, mes);
            for (int dia = 1; dia <= max_dia; ++dia) {
                std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", anio, mes, dia);
                fechas.emplace_back(buffer);
            }
        }
    }
    return fechas;
}

struct ResultadoConsultaDiaria {
    std::size_t consultas{ 0 }; // peticiones HTTP realizadas (incluye reintentos)
    std::size_t sin_dato{ 0 };  // fechas con HTTP 404: sin cotizacion (fin de semana o feriado)
    std::size_t fallidas{ 0 };  // fechas que siguieron fallando tras todos los reintentos
};

// Consulta GET /exchanges/{pais}/{fecha} para cada par (pais, fecha) en paralelo.
// Es trabajo de E/S (espera de red), por eso se usan mas hilos que nucleos y schedule(dynamic).
// Cada iteracion escribe solo su propia posicion de 'resultados' (sin condiciones de carrera);
// el token compartido se lee y se renueva dentro de critical(acceso_token).
ResultadoConsultaDiaria consultarDiasEnParalelo(
    const std::vector<int>& paises,
    const std::vector<std::string>& calendario,
    const std::string& url_base,
    const std::string& email,
    const std::string& api_key,
    std::string& token_jwt,
    std::array<std::map<std::string, double>, CacheTasas::TOTAL_PAISES>& tasas_recibidas,
    int hilos) {

    const std::size_t n_dias = calendario.size();
    const std::size_t total = paises.size() * n_dias;
    std::vector<double> resultados(total, -1.0);
    std::size_t consultas = 0;
    std::size_t sin_dato = 0;
    std::size_t fallidas = 0;

    #pragma omp parallel num_threads(hilos) reduction(+ : consultas, sin_dato, fallidas)
    {
        ClienteHttp cliente_hilo;

        #pragma omp for schedule(dynamic, 4)
        for (std::size_t k = 0; k < total; ++k) {
            const int pais = paises[k / n_dias];
            const std::string& fecha = calendario[k % n_dias];
            const std::string url = url_base + "/exchanges/" + utem::mapearCodigoPais(pais) + "/" + fecha;

            bool resuelta = false;
            for (int intento = 0; intento < CacheTasas::MAX_INTENTOS_CONSULTA_DIARIA && !resuelta; ++intento) {
                if (intento > 0) {
                    // Espera creciente entre reintentos: 0,5 s, 1 s, 2 s...
                    std::this_thread::sleep_for(std::chrono::milliseconds(250L << intento));
                }

                std::string token_actual;
                #pragma omp critical(acceso_token)
                {
                    token_actual = token_jwt;
                }

                try {
                    ++consultas;
                    const nlohmann::json j = nlohmann::json::parse(cliente_hilo.get(url, token_actual));
                    resultados[k] = crear_tasa_desde_json(j).obtener_tasa();
                    resuelta = true;
                } catch (const ErrorHttp& e) {
                    if (e.codigo_http() == 404) {
                        ++sin_dato; // sin cotizacion ese dia: se resuelve despues con la politica de respaldo
                        resuelta = true;
                    } else if (e.codigo_http() == 403 || e.codigo_http() == 401) {
                        #pragma omp critical(acceso_token)
                        {
                            if (token_jwt == token_actual) {
                                try {
                                    token_jwt = cliente_hilo.autenticar(url_base, email, api_key);
                                } catch (const std::exception&) {
                                    // se reintenta en la siguiente vuelta
                                }
                            }
                        }
                    }
                } catch (const std::exception&) {
                    // JSON invalido o campo faltante: se reintenta
                }
            }
            if (!resuelta) {
                ++fallidas;
            }
        }
    }

    for (std::size_t k = 0; k < total; ++k) {
        if (resultados[k] > 0.0) {
            tasas_recibidas[static_cast<std::size_t>(paises[k / n_dias])][calendario[k % n_dias]] = resultados[k];
        }
    }
    return { consultas, sin_dato, fallidas };
}
} // namespace

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
    if (fecha.size() != 10 || fecha[4] != '-' || fecha[7] != '-') {
        throw std::invalid_argument(
            "Formato de fecha invalido en TasaCambio. Se esperaba YYYY-MM-DD, se recibio: " + fecha);
    }
    for (std::size_t i : { 0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u }) {
        if (fecha[i] < '0' || fecha[i] > '9') {
            throw std::invalid_argument(
                "Formato de fecha invalido en TasaCambio. Se esperaba YYYY-MM-DD, se recibio: " + fecha);
        }
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

int CacheTasas::calcular_indice_dia_2023_2025(const std::string& fecha) noexcept {
    if (fecha.size() < 10 || fecha[4] != '-' || fecha[7] != '-') {
        return -1;
    }
    const int anio = (fecha[0] - '0') * 1000 + (fecha[1] - '0') * 100 + (fecha[2] - '0') * 10 + (fecha[3] - '0');
    const int mes = (fecha[5] - '0') * 10 + (fecha[6] - '0');
    const int dia = (fecha[8] - '0') * 10 + (fecha[9] - '0');

    if (anio < 2023 || anio > 2025 || mes < 1 || mes > 12 || dia < 1 || dia > diasEnMes(anio, mes)) {
        return -1;
    }

    static constexpr int acumulado_mes_normal[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    static constexpr int acumulado_mes_bisiesto[] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

    int offset_anio = 0;
    if (anio == 2024) {
        offset_anio = 365;
    } else if (anio == 2025) {
        offset_anio = 365 + 366;
    }

    const int offset_mes = (anio == 2024) ? acumulado_mes_bisiesto[mes - 1] : acumulado_mes_normal[mes - 1];
    return offset_anio + offset_mes + (dia - 1);
}

void CacheTasas::descargar_tasas_por_rango(
    const std::string& url_base,
    const std::string& email,
    const std::string& api_key,
    std::string& token_jwt,
    int num_hilos,
    const std::string& fecha_inicio,
    const std::string& fecha_fin) {

    std::array<std::map<std::string, double>, TOTAL_PAISES> tasas_recibidas;
    std::array<std::string, TOTAL_PAISES> errores_pais;
    std::size_t contador_peticiones = 0;

    const int hilos_consulta = std::max(1, std::min(num_hilos, static_cast<int>(TOTAL_PAISES)));

    // 3 peticiones en total (una por pais: 0=chile, 1=colombia, 2=ecuador)
    #pragma omp parallel num_threads(hilos_consulta)
    {
        ClienteHttp cliente_hilo;

        #pragma omp for schedule(dynamic, 1)
        for (int codigo_pais = 0; codigo_pais < static_cast<int>(TOTAL_PAISES); ++codigo_pais) {
            const std::string nombre_pais = utem::mapearCodigoPais(codigo_pais);
            const std::string url = url_base + "/exchanges/" + nombre_pais
                + "?start_date=" + fecha_inicio + "&end_date=" + fecha_fin;

            bool exito = false;
            std::string ultimo_error;

            for (int intento = 0; intento < 3; ++intento) {
                std::string token_actual;
                #pragma omp critical(acceso_token)
                {
                    token_actual = token_jwt;
                }

                try {
                    #pragma omp atomic
                    ++contador_peticiones;

                    const std::string cuerpo = cliente_hilo.get(url, token_actual);
                    const nlohmann::json j = nlohmann::json::parse(cuerpo);

                    if (!j.is_array()) {
                        throw Error("Se esperaba un arreglo JSON en la consulta por rango para " + nombre_pais);
                    }

                    for (const auto& item : j) {
                        const TasaCambio tc = crear_tasa_desde_json(item);
                        tasas_recibidas[codigo_pais][tc.obtener_fecha()] = tc.obtener_tasa();
                    }

                    exito = true;
                    break;
                } catch (const ErrorHttp& e) {
                    ultimo_error = e.what();
                    // La API responde HTTP 403 cuando el token es invalido o expiro (y 401 por compatibilidad)
                    if (e.codigo_http() == 403 || e.codigo_http() == 401) {
                        #pragma omp critical(acceso_token)
                        {
                            if (token_jwt == token_actual) {
                                try {
                                    token_jwt = cliente_hilo.autenticar(url_base, email, api_key);
                                } catch (const std::exception& err_login) {
                                    ultimo_error = std::string("Fallo al renovar JWT tras HTTP ")
                                        + std::to_string(e.codigo_http()) + ": " + err_login.what();
                                }
                            }
                        }
                    } else if (e.codigo_http() == 404) {
                        break;
                    }
                } catch (const std::exception& e) {
                    ultimo_error = e.what();
                }
            }

            if (!exito || tasas_recibidas[codigo_pais].empty()) {
                errores_pais[codigo_pais] = "No se pudieron obtener tasas por rango para '" + nombre_pais
                    + "': " + ultimo_error;
            }
        }
    }

    const std::vector<std::string> calendario = generarFechasPeriodo2023A2025();

    // Respaldo: si la consulta por rango falla para un pais (por ejemplo, HTTP 500 del servidor),
    // se consultan sus dias uno por uno con GET /exchanges/{pais}/{fecha}, en paralelo.
    std::vector<int> paises_respaldo;
    for (int p = 0; p < static_cast<int>(TOTAL_PAISES); ++p) {
        if (!errores_pais[p].empty()) {
            std::cerr << "[AVISO] " << errores_pais[p] << ". Se consultara dia por dia.\n";
            paises_respaldo.push_back(p);
        }
    }

    consultas_diarias_ = 0;
    dias_sin_dato_ = 0;
    consultas_diarias_fallidas_ = 0;
    if (!paises_respaldo.empty()) {
        const ResultadoConsultaDiaria r = consultarDiasEnParalelo(
            paises_respaldo, calendario, url_base, email, api_key, token_jwt, tasas_recibidas,
            std::max(num_hilos, HILOS_CONSULTA_DIARIA));
        consultas_diarias_ = r.consultas;
        dias_sin_dato_ = r.sin_dato;
        consultas_diarias_fallidas_ = r.fallidas;
        contador_peticiones += r.consultas;

        if (r.fallidas > 0) {
            std::cerr << "[AVISO] " << r.fallidas << " fechas no se pudieron consultar tras "
                      << MAX_INTENTOS_CONSULTA_DIARIA << " intentos; se tratan como fechas sin tasa.\n";
        }
        for (const int p : paises_respaldo) {
            if (tasas_recibidas[static_cast<std::size_t>(p)].empty()) {
                throw Error("No se obtuvo ninguna tasa para '" + utem::mapearCodigoPais(p)
                    + "' ni por rango ni dia por dia: " + errores_pais[static_cast<std::size_t>(p)]);
            }
        }
    }

    peticiones_http_ = contador_peticiones;
    tasas_.clear();
    fechas_con_respaldo_ = 0;

    // Poblar cache y aplicar politica para fechas sin tasa disponible (fines de semana / feriados):
    // Se usa la ultima tasa disponible anterior del mismo pais (o la primera posterior si es al inicio del periodo).
    for (int pais = 0; pais < static_cast<int>(TOTAL_PAISES); ++pais) {
        const auto& mapa_api = tasas_recibidas[pais];
        for (std::size_t idx = 0; idx < calendario.size(); ++idx) {
            const std::string& fecha = calendario[idx];
            double tasa_final = -1.0;

            const auto it_exacto = mapa_api.find(fecha);
            if (it_exacto != mapa_api.end()) {
                tasa_final = it_exacto->second;
            } else {
                auto it_sup = mapa_api.lower_bound(fecha);
                if (it_sup != mapa_api.begin()) {
                    tasa_final = std::prev(it_sup)->second;
                } else if (it_sup != mapa_api.end()) {
                    tasa_final = it_sup->second;
                }
                ++fechas_con_respaldo_;
            }

            if (tasa_final <= 0.0) {
                throw Error("No existe tasa de respaldo para el pais " + std::to_string(pais) + " en fecha " + fecha);
            }

            tabla_rapida_[pais][idx] = tasa_final;
            tasas_[{ pais, fecha }] = tasa_final;
        }
    }
}

double CacheTasas::obtener_tasa(int codigo_pais, const std::string& fecha) const {
    if (codigo_pais >= 0 && codigo_pais < static_cast<int>(TOTAL_PAISES)) {
        const int idx = calcular_indice_dia_2023_2025(fecha);
        if (idx >= 0 && static_cast<std::size_t>(idx) < DIAS_PERIODO_2023_2025) {
            const double valor = tabla_rapida_[codigo_pais][idx];
            if (valor > 0.0) {
                return valor;
            }
        }
    }

    const auto it = tasas_.find({ codigo_pais, fecha });
    if (it == tasas_.end()) {
        throw Error("Tasa no encontrada en cache para pais=" + std::to_string(codigo_pais) + " fecha=" + fecha);
    }
    return it->second;
}

std::size_t CacheTasas::total_tasas() const noexcept {
    return tasas_.size();
}

std::size_t CacheTasas::total_peticiones_http() const noexcept {
    return peticiones_http_;
}

std::size_t CacheTasas::total_fechas_respaldo() const noexcept {
    return fechas_con_respaldo_;
}

std::size_t CacheTasas::total_consultas_diarias() const noexcept {
    return consultas_diarias_;
}

std::size_t CacheTasas::total_dias_sin_dato() const noexcept {
    return dias_sin_dato_;
}

std::size_t CacheTasas::total_consultas_diarias_fallidas() const noexcept {
    return consultas_diarias_fallidas_;
}
