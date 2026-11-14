# CPYD Tarea 1: ventas multinacionales a USD con OpenMP

Computación Paralela y Distribuida, UTEM. Profesor: Sebastián Salazar Molina.
Enunciado: [`docs/enunciado/enunciado.pdf`](docs/enunciado/enunciado.pdf). Referencia de la API: [`docs/API.md`](docs/API.md). Resumen y plan: [`docs/PANORAMA.md`](docs/PANORAMA.md).

Programa en C++17 con OpenMP que procesa archivos masivos de transacciones de ventas de Chile, Colombia y Ecuador sin cargar el archivo completo en memoria, filtra las ventas del `01/01/2023` al `31/12/2025`, consulta las tasas de cambio por rango a la API autenticada (3 peticiones en total, una por país), convierte cada monto a USD y genera el resumen ordenado por país e identificador de cliente.

## Integrantes y participación

| Nombre | GitHub | Aporte |
|---|---|---|
| Renato Moyano | `imdaichi` | Entorno contenedorizado GCC 14.2.0 (`Containerfile`, `scripts/gcc14.sh`), sistema de compilación (`Makefile`), estructura del repositorio, flujo Gitflow y pruebas de integración |
| Benjamín Quilodrán | `bquilodran` | Paralelización de lectura por particiones de archivo sin carga completa en memoria, consulta de tasas por rango (3 peticiones), renovación de token ante HTTP `403`, benchmark (1, 2, 4 y 8 hilos) y documentación técnica |
| Cristobal Huentecura | `WENTE57` | Implementación de módulos base en C++ (`ClienteHttp` con `libcurl`, `ConfiguracionEntorno`, `Venta`, `TasaCambio`, `ProcesadorVentas`) y validación de datos |

## Estructura del repositorio

```
CPYD-TAREA-1/
├── src/              código fuente C++ (main, lectura paralela y validación, cliente HTTP, caché de tasas)
├── third_party/      nlohmann/json (json.hpp)
├── scripts/          gcc14.sh (contenedor GCC 14.2.0) y benchmark.sh (medición con 1, 2, 4 y 8 hilos)
├── tests/            archivos CSV chicos de prueba (tests/ventas_prueba.csv con casos válidos e inválidos)
├── data/             ventas.csv del profesor (~14 GB, no se sube al repositorio)
├── results/          tiempos del benchmark (results/benchmark_tiempos.csv)
├── docs/             enunciado, API.md y documentación de apoyo
├── Makefile          compila src/ en build/ y genera ./programa
├── Containerfile     imagen de compilación gcc:14.2.0 + libcurl
├── .env.example      plantilla de credenciales (copiar a .env, que no se sube)
└── .clang-format     estilo de código
```

`build/`, `programa`, `data/`, `.env` y las salidas (`resumen_ventas_usd.csv`, `errores.csv`) están en `.gitignore`.

## Requisitos

El enunciado exige que compile en **GCC 14.2.0**. Para que todo el grupo use exactamente esa versión, sin importar la que traiga su sistema, se compila dentro de un contenedor basado en la imagen oficial `gcc:14.2.0` ([`Containerfile`](Containerfile)), que ya incluye `g++`, OpenMP (`libgomp`) y `libcurl`.

- Podman (viene en Fedora) o Docker
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, incluido en `third_party/json.hpp`

```bash
scripts/gcc14.sh gcc --version     # la primera vez construye la imagen; debe decir 14.2.0
scripts/gcc14.sh                   # abre una terminal dentro del contenedor
```

## Credenciales

El token JWT **no** se guarda en el repositorio ni se hardcodea en el código: el programa se autentica al iniciar usando el correo institucional UTEM y el RUT leídos desde `.env` o variables de entorno.

```bash
cp .env.example .env      # .env está en .gitignore
nano .env                 # completar CPYD_EMAIL y CPYD_APIKEY (RUT con puntos y guion)
```

## Compilación y ejecución

```bash
# Compilar con GCC 14.2.0 y -fopenmp
scripts/gcc14.sh make

# Ejecución normal con 8 hilos
scripts/gcc14.sh ./programa --input data/ventas.csv --output resumen_ventas_usd.csv --threads 8

# Ejecución de prueba con el dataset reducido incluido en el repositorio
scripts/gcc14.sh ./programa --input tests/ventas_prueba.csv --output resumen_ventas_usd.csv --threads 4

# Ejecutar benchmark automático con 1, 2, 4 y 8 hilos
scripts/gcc14.sh ./programa --input data/ventas.csv --output resumen_ventas_usd.csv --benchmark
```

`scripts/gcc14.sh` carga automáticamente el `.env` dentro del contenedor.

| Opción | Descripción | Por defecto |
|---|---|---|
| `--input` | archivo CSV de transacciones de entrada | `ventas.csv` |
| `--output` | CSV de salida con columnas `pais,id_cliente,total_usd` | `resumen_ventas_usd.csv` |
| `--errors` | CSV donde se registran las filas inválidas o duplicadas | `errores.csv` |
| `--threads` | número de hilos OpenMP | `omp_get_max_threads()` |
| `--benchmark` | ejecuta la comparación de tiempos con 1, 2, 4 y 8 hilos | desactivado |

## Flujo de trabajo (Gitflow)

| Rama | Para qué | Sale de | Se une a |
|---|---|---|---|
| `main` | Solo versiones entregables, cada una con su tag (`v1.0.0`) | — | — |
| `develop` | Integración del trabajo del grupo (rama por defecto) | `main` | — |
| `feature/<tema>` | Una funcionalidad: `feature/lectura-csv`, `feature/cliente-api` | `develop` | `develop` |
| `release/<versión>` | Preparar una entrega: README, resultados, ajustes finales | `develop` | `main` y `develop` |
| `hotfix/<versión>` | Corrección urgente sobre lo ya entregado | `main` | `main` y `develop` |

## Formato de entrada

El archivo de transacciones contiene una línea por venta con 5 columnas separadas por `,` (o `;`), con encabezado opcional en la primera línea:

```csv
pais,id_cliente,id_boleta,fecha_hora,monto
0,bd9c66b3-ad3c-4d6d-9a3d-1fa7bc8960a9,ae340454-cac5-468c-a8f4-9481a0a04dc4,2025-02-11 21:42:52,32700.33
```

| Campo | Tipo | Regla de validación |
|---|---|---|
| `pais` | Entero | `0` = Chile (`chile`), `1` = Colombia (`colombia`), `2` = Ecuador (`ecuador`) |
| `id_cliente` | UUID | Formato `8-4-4-4-12` hexadecimal (36 caracteres). Identifica al cliente dentro de cada país |
| `id_boleta` | UUID | Formato `8-4-4-4-12` hexadecimal, **único en todo el archivo** |
| `fecha_hora` | Fecha y hora | Formato ISO consistente (`YYYY-MM-DD`, `YYYY-MM-DD HH:MM:SS` o con `T`), fecha calendario existente (valida años bisiestos y días por mes) y dentro del rango `2023-01-01` a `2025-12-31` inclusive |
| `monto` | Decimal | Número finito estrictamente positivo (`> 0.0`), en moneda local del país |

## Estrategia de paralelización

El diseño separa el problema en etapas pensadas para archivos de gran volumen (~14 GB) sin agotar la memoria RAM y evitando toda condición de carrera:

1. **Consulta paralela de tasas por rango (3 peticiones HTTP en total):**
   - En lugar de consultar a la API por cada venta o por cada día individual (~3.288 peticiones), se utiliza el endpoint de rango `GET /v1/exchanges/{country}?start_date=2023-01-01&end_date=2025-12-31`.
   - Se lanzan **exactamente 3 peticiones** en paralelo mediante `#pragma omp parallel` y `#pragma omp for schedule(dynamic, 1)` (una para `chile`, una para `colombia` y una para `ecuador`), donde cada hilo posee su propia instancia `ClienteHttp` (`CURL*` independiente).
   - Con las respuestas se construye una tabla de búsqueda indexada en tiempo constante $O(1)$ (`tabla_rapida_[3][1096]`) que cubre los 3 países y los 1.096 días del período `2023-01-01`..`2025-12-31`. Durante el procesamiento de ventas esta tabla es de **solo lectura**, eliminando cualquier bloqueo al consultar tasas.

2. **Lectura paralela del archivo CSV por particiones de bytes (sin cargar el archivo completo en memoria):**
   - El programa obtiene el tamaño en bytes del archivo (`tam_archivo`) y divide el archivo en $P = \text{num\_hilos}$ segmentos contiguos `[inicio_nominal, fin_nominal)`.
   - Cada hilo OpenMP abre su propio descriptor `std::ifstream` con un buffer de E/S local de 1 MB. Si `id_hilo > 0`, el hilo se posiciona en `inicio_nominal - 1` y avanza hasta el primer salto de línea `\n`, garantizando que cada línea sea procesada por un único hilo sin cortes ni duplicaciones.
   - Cada hilo lee, valida, convierte a USD y acumula **línea por línea en streaming**, descartando inmediatamente la fila procesada. Así no se almacena un `std::vector<Venta>` en memoria y el consumo de RAM se mantiene acotado sin importar si el archivo pesa cientos de megabytes o 14 GB.

3. **Detección paralela de boletas duplicadas con particionamiento de locks (256 shards):**
   - Cada UUID de boleta se convierte al validarse en una estructura binaria compacta de 128 bits (`Uuid128`, 16 bytes en lugar de un `std::string` de 36 bytes en el heap).
   - Para verificar unicidad global entre hilos sin un cuello de botella central, las boletas vistas se almacenan en un arreglo de **256 particiones (`shards`)**, cada una protegida por su propio candado `omp_lock_t` indexado por los bits bajos de `uuid.hi`.

4. **Acumulación por hilo y reducción final sin condiciones de carrera:**
   - Cada hilo acumula los montos convertidos ($\text{monto\_USD} = \text{monto\_local} / \text{rate}$) en un mapa privado `std::map<std::pair<int, std::string>, double> acumulador_local`.
   - Al finalizar la lectura de su segmento, cada hilo fusiona su mapa local en el mapa global `totales_por_cliente_` dentro de una única sección `#pragma omp critical(mezcla_totales)`. Al ser `std::map` ordenado por clave `(pais, id_cliente)`, la salida queda automáticamente ordenada para su escritura a CSV.

## Política de errores

1. **Filas inválidas y boletas duplicadas:**
   - Toda fila con número incorrecto de columnas, código de país fuera de `{0, 1, 2}`, UUID mal formado, fecha calendario inexistente (ej. `2024-02-30`) o monto `<= 0` es ignorada del cálculo y registrada en `errores.csv` indicando `linea,motivo,contenido`.
   - Si un `id_boleta` se repite en cualquier parte del archivo, se conserva la primera aparición válida y las repeticiones posteriores se rechazan y registran en `errores.csv` con el motivo `"Boleta duplicada: <uuid>"`.
   - Las ventas válidas cuya fecha esté fuera del rango `2023-01-01`..`2025-12-31` se excluyen del cálculo según el requisito de filtrado temporal, pero su boleta igualmente se registra como vista para impedir reutilización de boletas.

2. **Errores HTTP, timeouts y renovación de token en error `403`:**
   - Todas las peticiones HTTP con `libcurl` configuran timeouts de conexión (17 s) y total (47 s), con hasta 3 reintentos por petición.
   - Cuando la API responde con código **HTTP `403`** (token ausente, inválido o expirado tras los 67 minutos de validez, o `401`), el hilo entra en `#pragma omp critical(acceso_token)`, verifica si el token compartido aún no ha sido renovado por otro hilo y ejecuta `POST /v1/login/authenticate` una sola vez antes de reintentar la consulta.

3. **Fechas sin tasa disponible (fines de semana y feriados):**
   - La API solo publica cotizaciones en días hábiles (omitiendo sábados, domingos y feriados).
   - Política documentada: al construir la caché del período `2023-01-01`..`2025-12-31`, para toda fecha sin cotización en un país se asigna la **última tasa disponible anterior** de ese mismo país. Si el primer día del período (`2023-01-01`, domingo) no tiene tasa anterior en el rango, se utiliza la primera tasa hábil inmediatamente posterior del mismo país (`2023-01-02`).

## Resultados y análisis de rendimiento

Se evaluó el desempeño del procesamiento paralelo variando el número de hilos OpenMP ($p \in \{1, 2, 4, 8\}$), midiendo el tiempo con `omp_get_wtime()` y verificando con `diff` que el archivo `resumen_ventas_usd.csv` generado con 1 hilo y con 8 hilos sea idéntico.

Las métricas evaluadas son:
- **Speedup:** $S(p) = \frac{T(1)}{T(p)}$
- **Eficiencia:** $E(p) = \frac{S(p)}{p}$

| Hilos ($p$) | Tiempo lectura + conversión (s) | Tiempo escritura CSV (s) | Tiempo total procesamiento (s) | Speedup $S(p)$ | Eficiencia $E(p)$ |
|---:|---:|---:|---:|---:|---:|
| 1 | 48.6210 | 0.1420 | 48.7630 | 1.0000x | 100.00% |
| 2 | 25.4180 | 0.1410 | 25.5590 | 1.9079x | 95.39% |
| 4 | 13.6840 | 0.1430 | 13.8270 | 3.5266x | 88.17% |
| 8 | 7.8920 | 0.1420 | 8.0340 | 6.0696x | 75.87% |

### Análisis de resultados

1. **Escalabilidad y Speedup:** Al pasar de 1 a 2 y 4 hilos se obtiene un speedup cercano al ideal ($1.91\times$ y $3.53\times$, con eficiencias del $95.39\%$ y $88.17\%$), gracias a que cada hilo lee y parsea un segmento independiente del archivo sin usar expresiones regulares lentas (`std::regex`) ni flujos con bloqueo de `std::locale`, y acumula en un mapa local sin contención.
2. **Impacto de las 3 consultas por rango:** Al reemplazar miles de consultas diarias individuales por solo **3 peticiones por rango** (una por país), el tiempo de red frente a la API se reduce a ~0.4–0.8 segundos al inicio y deja de ser un cuello de botella durante el recorrido del archivo.
3. **Ley de Amdahl en 8 hilos:** Con 8 hilos el speedup alcanza $6.07\times$ ($75.87\%$ de eficiencia). La desviación respecto al speedup lineal ideal se explica por el ancho de banda de lectura del almacenamiento secundario (E/S de disco compartida entre los 8 flujos `std::ifstream`), la sincronización ligera en los 256 shards de boletas y las fracciones secuenciales (autenticación inicial, mezcla final de mapas por hilo y escritura ordenada de `resumen_ventas_usd.csv` y `errores.csv`).

## Supuestos

1. El archivo de entrada utiliza codificación ASCII/UTF-8 con saltos de línea `\n` o `\r\n`, separador de columnas `,` (o `;`) y punto `.` como separador decimal.
2. Si la primera línea del archivo no comienza con un dígito ni con el signo `-`, se interpreta como encabezado CSV y se omite automáticamente.
3. Los montos acumulados en USD se almacenan en `double` durante la reducción y se formatean a 2 decimales fijos (`std::fixed` + `std::setprecision(2)`) al escribir `resumen_ventas_usd.csv`.
