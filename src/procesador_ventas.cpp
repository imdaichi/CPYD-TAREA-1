#include "procesador_ventas.h"
#include "error.h"
#include "utem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <omp.h>
#include <vector>

namespace {
// Las boletas (~119 millones) y los clientes (~25 millones) no caben en RAM con std::unordered_set,
// std::map y claves std::string (decenas de GB). Se usan tablas hash de direccionamiento abierto
// con el UUID guardado como 128 bits, repartidas en particiones con un candado cada una (lock striping):
// todos los hilos comparten una sola tabla, sin copias por hilo, y casi nunca compiten por el mismo candado.
constexpr std::size_t NUM_PARTICIONES = 1024; // potencia de 2
constexpr int BITS_PARTICION = 10;            // log2(NUM_PARTICIONES)
constexpr std::size_t CAPACIDAD_INICIAL = 1024;
constexpr std::size_t MAX_ERRORES_EN_BUFFER_POR_HILO = 50000;
constexpr double MICRO_POR_USD = 1000000.0;

std::uint64_t mezclar(std::uint64_t x) noexcept { // finalizador de splitmix64
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

std::uint64_t hashUuid(const Uuid128& u, std::uint64_t semilla) noexcept {
    return mezclar(u.hi ^ mezclar(u.lo + semilla));
}

std::size_t indiceParticion(std::uint64_t h) noexcept {
    return static_cast<std::size_t>(h >> (64 - BITS_PARTICION)); // bits altos: particion; bajos: ranura
}

bool esUuidCero(const Uuid128& u) noexcept {
    return u.hi == 0 && u.lo == 0;
}

// Conjunto de UUID de boletas: 16 bytes por ranura, sondeo lineal, crece al 80 % de ocupacion.
class ConjuntoBoletas {
public:
    static constexpr std::uint64_t SEMILLA = 0x0b01e7a5ULL;

    // Devuelve true si la boleta no estaba (primera aparicion).
    bool insertar(const Uuid128& u, std::uint64_t h) {
        if (esUuidCero(u)) { // la ranura vacia se marca con el UUID cero
            const bool nueva = !tiene_cero_;
            tiene_cero_ = true;
            return nueva;
        }
        if ((usadas_ + 1) * 10 > ranuras_.size() * 8) {
            crecer();
        }
        const std::size_t mascara = ranuras_.size() - 1;
        for (std::size_t i = static_cast<std::size_t>(h) & mascara;; i = (i + 1) & mascara) {
            Uuid128& r = ranuras_[i];
            if (esUuidCero(r)) {
                r = u;
                ++usadas_;
                return true;
            }
            if (r == u) {
                return false;
            }
        }
    }

private:
    void crecer() {
        std::vector<Uuid128> anteriores(std::max(CAPACIDAD_INICIAL, ranuras_.size() * 2));
        anteriores.swap(ranuras_);
        const std::size_t mascara = ranuras_.size() - 1;
        for (const Uuid128& u : anteriores) {
            if (esUuidCero(u)) {
                continue;
            }
            std::size_t i = static_cast<std::size_t>(hashUuid(u, SEMILLA)) & mascara;
            while (!esUuidCero(ranuras_[i])) {
                i = (i + 1) & mascara;
            }
            ranuras_[i] = u;
        }
    }

    std::vector<Uuid128> ranuras_;
    std::size_t usadas_{ 0 };
    bool tiene_cero_{ false };
};

// Totales por (pais, cliente) en micro-dolares: 32 bytes por ranura; pais = -1 marca ranura vacia.
class TablaClientes {
public:
    static constexpr std::uint64_t SEMILLA = 0xc11e47e5ULL;

    static std::uint64_t hash(int pais, const Uuid128& id) noexcept {
        return hashUuid(id, SEMILLA + static_cast<std::uint64_t>(pais));
    }

    void sumar(int pais, const Uuid128& id, std::uint64_t h, std::int64_t micro_usd) {
        if ((usadas_ + 1) * 10 > ranuras_.size() * 8) {
            crecer();
        }
        const std::size_t mascara = ranuras_.size() - 1;
        for (std::size_t i = static_cast<std::size_t>(h) & mascara;; i = (i + 1) & mascara) {
            TotalCliente& r = ranuras_[i];
            if (r.pais < 0) {
                r = TotalCliente{ pais, id, micro_usd };
                ++usadas_;
                return;
            }
            if (r.pais == pais && r.id_cliente == id) {
                r.micro_usd += micro_usd;
                return;
            }
        }
    }

    void volcar_en(std::vector<TotalCliente>& destino) const {
        for (const TotalCliente& r : ranuras_) {
            if (r.pais >= 0) {
                destino.push_back(r);
            }
        }
    }

    [[nodiscard]] std::size_t tamano() const noexcept {
        return usadas_;
    }

private:
    void crecer() {
        std::vector<TotalCliente> anteriores(std::max(CAPACIDAD_INICIAL, ranuras_.size() * 2), TotalCliente{ -1, {}, 0 });
        anteriores.swap(ranuras_);
        const std::size_t mascara = ranuras_.size() - 1;
        for (const TotalCliente& r : anteriores) {
            if (r.pais < 0) {
                continue;
            }
            std::size_t i = static_cast<std::size_t>(hash(r.pais, r.id_cliente)) & mascara;
            while (ranuras_[i].pais >= 0) {
                i = (i + 1) & mascara;
            }
            ranuras_[i] = r;
        }
    }

    std::vector<TotalCliente> ranuras_;
    std::size_t usadas_{ 0 };
};

struct ParticionBoletas {
    omp_lock_t lock{};
    ConjuntoBoletas boletas;
};

struct ParticionClientes {
    omp_lock_t lock{};
    TablaClientes clientes;
};

std::string uuidATexto(const Uuid128& u) {
    static constexpr char HEX[] = "0123456789abcdef";
    std::string texto;
    texto.reserve(36);
    for (int n = 0; n < 32; ++n) {
        if (n == 8 || n == 12 || n == 16 || n == 20) {
            texto.push_back('-');
        }
        const std::uint64_t palabra = (n < 16) ? u.hi : u.lo;
        const int desplazamiento = (15 - (n % 16)) * 4;
        texto.push_back(HEX[(palabra >> desplazamiento) & 0xFULL]);
    }
    return texto;
}

struct IncidenciaCsv {
    std::size_t linea_relativa{ 0 };
    std::string motivo;
    std::string contenido;
};

bool esEncabezadoCsv(const std::string& linea_limpia) noexcept {
    if (linea_limpia.empty()) {
        return false;
    }
    const char primero = linea_limpia[0];
    return !std::isdigit(static_cast<unsigned char>(primero)) && primero != '-';
}
} // namespace

void ProcesadorVentas::procesar_archivo_paralelo(
    const std::string& ruta_entrada,
    const std::string& ruta_errores,
    const CacheTasas& cache_tasas,
    int num_hilos) {

    std::ifstream probe(ruta_entrada, std::ios::binary | std::ios::ate);
    if (!probe.is_open()) {
        throw Error("No se pudo abrir el archivo de transacciones de entrada: " + ruta_entrada);
    }
    const auto pos_fin = probe.tellg();
    if (pos_fin < 0) {
        throw Error("No se pudo determinar el tamano del archivo de entrada: " + ruta_entrada);
    }
    const std::uint64_t tam_archivo = static_cast<std::uint64_t>(pos_fin);
    probe.close();

    std::ofstream archivo_errores(ruta_errores, std::ios::out | std::ios::trunc);
    if (!archivo_errores.is_open()) {
        throw Error("No se pudo abrir el archivo de registro de errores: " + ruta_errores);
    }
    archivo_errores << "linea,motivo,contenido\n";

    totales_por_cliente_.clear();
    ventas_validas_count_ = 0;
    registros_invalidos_ = 0;
    registros_fuera_rango_ = 0;

    if (tam_archivo == 0) {
        archivo_errores.flush();
        return;
    }

    std::vector<ParticionBoletas> particiones_boletas(NUM_PARTICIONES);
    std::vector<ParticionClientes> particiones_clientes(NUM_PARTICIONES);
    for (std::size_t i = 0; i < NUM_PARTICIONES; ++i) {
        omp_init_lock(&particiones_boletas[i].lock);
        omp_init_lock(&particiones_clientes[i].lock);
    }

    const int hilos = (num_hilos > 0) ? num_hilos : 1;
    std::vector<std::size_t> lineas_por_hilo(static_cast<std::size_t>(hilos), 0);
    std::vector<std::vector<IncidenciaCsv>> errores_por_hilo(static_cast<std::size_t>(hilos));
    std::string error_lectura_hilo;

    #pragma omp parallel num_threads(hilos)
    {
        const int id_hilo = omp_get_thread_num();
        const int total_hilos = omp_get_num_threads();

        const std::uint64_t inicio_nominal = (tam_archivo * static_cast<std::uint64_t>(id_hilo))
            / static_cast<std::uint64_t>(total_hilos);
        const std::uint64_t fin_nominal = (tam_archivo * static_cast<std::uint64_t>(id_hilo + 1))
            / static_cast<std::uint64_t>(total_hilos);

        std::ifstream archivo_hilo(ruta_entrada, std::ios::binary);
        std::vector<char> buffer_io(1024 * 1024);
        archivo_hilo.rdbuf()->pubsetbuf(buffer_io.data(), static_cast<std::streamsize>(buffer_io.size()));

        if (!archivo_hilo.is_open()) {
            #pragma omp critical(error_critico)
            {
                if (error_lectura_hilo.empty()) {
                    error_lectura_hilo = "No se pudo abrir el archivo en el hilo " + std::to_string(id_hilo);
                }
            }
        } else if (inicio_nominal < fin_nominal) {
            std::uint64_t pos_actual = 0;

            if (id_hilo > 0 && inicio_nominal > 0) {
                archivo_hilo.seekg(static_cast<std::streamoff>(inicio_nominal - 1), std::ios::beg);
                std::string descarte;
                if (std::getline(archivo_hilo, descarte, '\n')) {
                    pos_actual = inicio_nominal + static_cast<std::uint64_t>(descarte.size());
                } else {
                    pos_actual = fin_nominal;
                }
            }

            std::size_t validas_local = 0;
            std::size_t invalidas_local = 0;
            std::size_t fuera_rango_local = 0;
            std::size_t contador_lineas = 0;
            auto& buffer_errores = errores_por_hilo[static_cast<std::size_t>(id_hilo)];

            std::string linea;
            while (pos_actual < fin_nominal && std::getline(archivo_hilo, linea, '\n')) {
                pos_actual += static_cast<std::uint64_t>(linea.size()) + 1ULL;
                ++contador_lineas;

                const std::string linea_limpia = utem::eliminarEspacios(linea);
                if (linea_limpia.empty()) {
                    continue;
                }
                if (id_hilo == 0 && contador_lineas == 1 && esEncabezadoCsv(linea_limpia)) {
                    continue;
                }

                const char separador = (linea_limpia.find(';') != std::string::npos) ? ';' : ',';

                try {
                    const Venta venta = Venta::desde_linea_csv(linea_limpia, separador);
                    const Uuid128 uuid_boleta = venta.obtener_uuid_boleta();
                    const std::uint64_t h_boleta = hashUuid(uuid_boleta, ConjuntoBoletas::SEMILLA);
                    ParticionBoletas& pb = particiones_boletas[indiceParticion(h_boleta)];

                    omp_set_lock(&pb.lock);
                    const bool es_duplicada = !pb.boletas.insertar(uuid_boleta, h_boleta);
                    omp_unset_lock(&pb.lock);

                    if (es_duplicada) {
                        ++invalidas_local;
                        const std::string motivo = "Boleta duplicada: " + venta.obtener_id_boleta();
                        if (buffer_errores.size() < MAX_ERRORES_EN_BUFFER_POR_HILO) {
                            buffer_errores.push_back(IncidenciaCsv{ contador_lineas, motivo, linea_limpia });
                        } else {
                            #pragma omp critical(escritura_errores)
                            {
                                archivo_errores << contador_lineas << ",\"" << motivo << "\",\"" << linea_limpia << "\"\n";
                            }
                        }
                        continue;
                    }

                    if (!Venta::esta_en_rango_2023_2025(venta.obtener_fecha())) {
                        ++fuera_rango_local;
                        continue;
                    }

                    const double rate = cache_tasas.obtener_tasa(venta.obtener_pais(), venta.obtener_fecha());
                    const double monto_usd = venta.obtener_monto_local() / rate;
                    const std::int64_t micro_usd = std::llround(monto_usd * MICRO_POR_USD);
                    const Uuid128 uuid_cliente = venta.obtener_uuid_cliente();
                    const std::uint64_t h_cliente = TablaClientes::hash(venta.obtener_pais(), uuid_cliente);
                    ParticionClientes& pc = particiones_clientes[indiceParticion(h_cliente)];

                    omp_set_lock(&pc.lock);
                    pc.clientes.sumar(venta.obtener_pais(), uuid_cliente, h_cliente, micro_usd);
                    omp_unset_lock(&pc.lock);
                    ++validas_local;
                } catch (const std::exception& e) {
                    ++invalidas_local;
                    if (buffer_errores.size() < MAX_ERRORES_EN_BUFFER_POR_HILO) {
                        buffer_errores.push_back(IncidenciaCsv{ contador_lineas, e.what(), linea_limpia });
                    } else {
                        #pragma omp critical(escritura_errores)
                        {
                            archivo_errores << contador_lineas << ",\"" << e.what() << "\",\"" << linea_limpia << "\"\n";
                        }
                    }
                }
            }

            lineas_por_hilo[static_cast<std::size_t>(id_hilo)] = contador_lineas;

            #pragma omp critical(mezcla_totales)
            {
                ventas_validas_count_ += validas_local;
                registros_invalidos_ += invalidas_local;
                registros_fuera_rango_ += fuera_rango_local;
            }
        }
    }

    for (std::size_t i = 0; i < NUM_PARTICIONES; ++i) {
        omp_destroy_lock(&particiones_boletas[i].lock);
        omp_destroy_lock(&particiones_clientes[i].lock);
    }
    particiones_boletas.clear();
    particiones_boletas.shrink_to_fit(); // libera la memoria de las boletas antes de ordenar

    std::size_t total_clientes = 0;
    for (const auto& pc : particiones_clientes) {
        total_clientes += pc.clientes.tamano();
    }
    totales_por_cliente_.reserve(total_clientes);
    for (const auto& pc : particiones_clientes) {
        pc.clientes.volcar_en(totales_por_cliente_);
    }
    particiones_clientes.clear();
    particiones_clientes.shrink_to_fit();

    // Orden de salida: pais y luego id_cliente. Comparar (hi, lo) del UUID equivale a comparar
    // su texto hexadecimal en minusculas.
    std::sort(totales_por_cliente_.begin(), totales_por_cliente_.end(),
        [](const TotalCliente& a, const TotalCliente& b) {
            if (a.pais != b.pais) {
                return a.pais < b.pais;
            }
            if (a.id_cliente.hi != b.id_cliente.hi) {
                return a.id_cliente.hi < b.id_cliente.hi;
            }
            return a.id_cliente.lo < b.id_cliente.lo;
        });

    if (!error_lectura_hilo.empty()) {
        throw Error(error_lectura_hilo);
    }

    // Volcar errores con su numero de linea global exacto en orden
    std::size_t offset_linea = 0;
    for (int h = 0; h < hilos; ++h) {
        const auto idx = static_cast<std::size_t>(h);
        for (const auto& inc : errores_por_hilo[idx]) {
            archivo_errores << (offset_linea + inc.linea_relativa)
                            << ",\"" << inc.motivo << "\",\"" << inc.contenido << "\"\n";
        }
        offset_linea += lineas_por_hilo[idx];
    }

    if (!archivo_errores.flush() || archivo_errores.fail()) {
        throw Error("Error de E/S al vaciar el buffer del archivo de errores: " + ruta_errores);
    }
}

void ProcesadorVentas::exportar_resumen_csv(const std::string& ruta_salida) const {
    std::ofstream archivo_csv(ruta_salida, std::ios::out | std::ios::trunc);
    if (!archivo_csv.is_open()) {
        throw Error("No se pudo abrir el archivo de salida para escritura: " + ruta_salida);
    }

    archivo_csv << "pais,id_cliente,total_usd\n";
    archivo_csv << std::setfill('0');

    for (const auto& item : totales_por_cliente_) {
        // micro-dolares -> centavos, redondeando al centavo mas cercano (los montos son positivos)
        const std::int64_t centavos = (item.micro_usd + 5000) / 10000;
        archivo_csv << item.pais << ',' << uuidATexto(item.id_cliente) << ','
                    << (centavos / 100) << '.' << std::setw(2) << (centavos % 100) << '\n';
    }

    if (!archivo_csv.flush() || archivo_csv.fail()) {
        throw Error("Error de E/S al escribir o vaciar el buffer del archivo CSV: " + ruta_salida);
    }
}

std::size_t ProcesadorVentas::total_ventas_validas() const noexcept {
    return ventas_validas_count_;
}

std::size_t ProcesadorVentas::total_registros_invalidos() const noexcept {
    return registros_invalidos_;
}

std::size_t ProcesadorVentas::total_fuera_de_rango() const noexcept {
    return registros_fuera_rango_;
}

std::size_t ProcesadorVentas::total_clientes_agrupados() const noexcept {
    return totales_por_cliente_.size();
}
