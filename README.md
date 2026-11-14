# CPYD Tarea 1: ventas multinacionales a USD con OpenMP

Computación Paralela y Distribuida, UTEM. Profesor: Sebastián Salazar Molina.
Enunciado: [`docs/enunciado/enunciado.pdf`](docs/enunciado/enunciado.pdf). Referencia de la API: [`docs/API.md`](docs/API.md). Resumen y plan: [`docs/PANORAMA.md`](docs/PANORAMA.md).

Programa en C++17 con OpenMP que procesa archivos masivos de transacciones de ventas de Chile, Colombia y Ecuador sin cargar el archivo completo en memoria, filtra las ventas del `01/01/2023` al `31/12/2025`, consulta las tasas de cambio por rango a la API autenticada (3 peticiones, una por país, con respaldo de consulta día por día en paralelo si la API falla), convierte cada monto a USD y genera el resumen ordenado por país e identificador de cliente.

## Integrantes y participación

| Nombre | GitHub | Aporte |
|---|---|---|
| Renato Moyano | `imdaichi` | Entorno contenedorizado GCC 14.2.0 (`Containerfile`, `scripts/gcc14.sh`), `Makefile`, tablas hash particionadas (`Uuid128` + *lock striping*) para soportar los 14 GB en RAM, respaldo de consulta diaria y pruebas |
| Benjamín Quilodrán | `bquilodran` | Paralelización de lectura por particiones de archivo en streaming, consulta de tasas por rango, renovación de token ante HTTP `403`, modo `--benchmark` y documentación de API |
| Cristobal Huentecura | `WENTE57` | Implementación de módulos base en C++ (`ClienteHttp` con reutilización de conexiones TLS Keep-Alive por hilo en `libcurl`, `ConfiguracionEntorno`, `Venta`, `TasaCambio`, `ProcesadorVentas`), validación de datos y ejecución de benchmarks sobre el dataset de 14 GB |

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

El archivo oficial de transacciones (`data/ventas.csv`, ~13,97 GB y ~119 millones de registros) contiene una línea por venta con 5 columnas separadas por `;` (el parser también soporta `,` automáticamente) y campos entre comillas dobles opcionales, con encabezado en la primera línea:

```csv
pais;cliente;boleta;fecha;monto
"2";"0e296426-ed5c-3408-b076-5da4c0c1ec63";"8432b0cc-af95-4824-88b4-b7b899745194";"2023-01-01 00:00:12";"254.9519"
"1";"f233766f-b091-3ae5-b968-6c762800dbbb";"66f00a24-d01b-4a9d-b2a2-1d07a5850fb6";"2023-01-01 00:00:43";"2427852.0000"
"0";"9df80d35-788c-3706-b4bd-4d3276f848ca";"03f2dc4f-359a-4ce3-b97c-6328acda282f";"2023-01-01 00:05:18";"34841.0000"
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

1. **Consulta paralela de tasas con reutilización de conexiones HTTP Keep-Alive:**
   - Se intenta primero el endpoint de rango `GET /v1/exchanges/{country}?start_date=2023-01-01&end_date=2025-12-31` en paralelo con `#pragma omp parallel` y `#pragma omp for schedule(dynamic, 1)` (una petición para `chile`, `colombia` y `ecuador`).
   - **Respaldo día por día con Keep-Alive:** cuando la consulta por rango responde HTTP `500`, el programa consulta automáticamente las 1.096 fechas de cada país (`3.288` pares `(país, fecha)` únicos en total) mediante `GET /v1/exchanges/{country}/{date}` en paralelo usando `#pragma omp parallel` con al menos 16 hilos (al ser una etapa *I/O-bound*), `schedule(dynamic, 4)` y `reduction(+:...)`.
   - **Optimización de transporte en `ClienteHttp`:** cada hilo instancia un único objeto `ClienteHttp` que mantiene su manejador `CURL*` vivo durante todo el bucle (usando `curl_easy_reset` y `CURLOPT_TCP_KEEPALIVE`), reutilizando la misma sesión TLS y conexión TCP abierta sin repetir el handshake SSL en cada fecha. Cada iteración escribe exclusivamente en su propia posición `resultados[k]`, eliminando condiciones de carrera.
   - Con las respuestas se construye una tabla de búsqueda indexada en tiempo constante $O(1)$ (`tabla_rapida_[3][1096]`) que cubre los 3 países y los 1.096 días del período `2023-01-01`..`2025-12-31`. Durante el procesamiento de ventas esta tabla es de **solo lectura**, eliminando cualquier bloqueo al consultar tasas.

2. **Lectura paralela del archivo CSV por particiones de bytes (sin cargar el archivo completo en memoria):**
   - El programa obtiene el tamaño en bytes del archivo (`tam_archivo`) y divide el archivo en $P = \text{num\_hilos}$ segmentos contiguos `[inicio_nominal, fin_nominal)`.
   - Cada hilo OpenMP abre su propio descriptor `std::ifstream` con un buffer de E/S local de 1 MB. Si `id_hilo > 0`, el hilo se posiciona en `inicio_nominal - 1` y avanza hasta el primer salto de línea `\n`, garantizando que cada línea sea procesada por un único hilo sin cortes ni duplicaciones.
   - Cada hilo lee, valida, convierte a USD y acumula **línea por línea en streaming**, descartando inmediatamente la fila procesada. Así no se almacena un `std::vector<Venta>` en memoria y el consumo de RAM se mantiene acotado sin importar si el archivo pesa cientos de megabytes o 14 GB.

3. **Tablas hash compartidas y particionadas con un candado por partición (lock striping):**
   - El archivo real tiene ~119 millones de boletas y `25.357.072` clientes agrupados por país. Con `std::unordered_set`, `std::map` y claves `std::string` (más un mapa local por hilo) el programa necesitaría más de 25 GB de RAM y el sistema terminaría el proceso por falta de memoria.
   - Los UUID de boleta y de cliente se convierten al validarse en `Uuid128` (16 bytes). Boletas y clientes se guardan en tablas hash de direccionamiento abierto con sondeo lineal (16 bytes por boleta y 32 bytes por cliente, crecen al 80 % de ocupación), repartidas en **1.024 particiones**. La partición se elige con los bits altos de un hash *splitmix64* del UUID y cada una tiene su propio candado `omp_lock_t`.
   - Todos los hilos comparten una sola tabla (no hay copias por hilo), y con 1.024 particiones dos hilos casi nunca compiten por el mismo candado.

4. **Acumulación sin condiciones de carrera y con resultado determinista:**
   - Cada venta se convierte ($\text{monto\_USD} = \text{monto\_local} / \text{rate}$), se redondea a micro-dólares (entero `int64`, USD × 1.000.000) y se suma al total de su cliente dentro del candado de su partición.
   - Como la suma de enteros no depende del orden, `resumen_ventas_usd.csv` sale idéntico con 1, 2, 4 u 8 hilos (con `double` los últimos decimales podían variar según el orden en que sumaban los hilos).
   - Al terminar, los totales se vuelcan a un vector, se ordenan por `(pais, id_cliente)` y se escriben en centavos redondeados a 2 decimales.

## Política de errores

1. **Filas inválidas y boletas duplicadas:**
   - Toda fila con número incorrecto de columnas, código de país fuera de `{0, 1, 2}`, UUID mal formado, fecha calendario inexistente (ej. `2024-02-30`) o monto `<= 0` es ignorada del cálculo y registrada en `errores.csv` indicando `linea,motivo,contenido`.
   - Si un `id_boleta` se repite en cualquier parte del archivo, se conserva la primera aparición válida y las repeticiones posteriores se rechazan y registran en `errores.csv` con el motivo `"Boleta duplicada: <uuid>"`.
   - Las ventas válidas cuya fecha esté fuera del rango `2023-01-01`..`2025-12-31` se excluyen del cálculo según el requisito de filtrado temporal, pero su boleta igualmente se registra como vista para impedir reutilización de boletas.

2. **Errores HTTP, timeouts y renovación de token en error `403`:**
   - Todas las peticiones HTTP con `libcurl` configuran timeouts de conexión (17 s) y total (47 s), con reintentos con espera creciente (*exponential backoff*).
   - Cuando la API responde con código **HTTP `403`** (token ausente, inválido o expirado tras los 67 minutos de validez, o `401`), el hilo entra en `#pragma omp critical(acceso_token)`, verifica si el token compartido aún no ha sido renovado por otro hilo y ejecuta `POST /v1/login/authenticate` una sola vez antes de reintentar la consulta.

3. **Fechas sin tasa disponible (fines de semana y feriados):**
   - La API solo publica cotizaciones en días hábiles (omitiendo sábados, domingos y feriados, para los cuales responde HTTP `404`).
   - Política documentada: al construir la caché del período `2023-01-01`..`2025-12-31`, para toda fecha sin cotización en un país se asigna la **última tasa disponible anterior** de ese mismo país. Si el primer día del período (`2023-01-01`, domingo) no tiene tasa anterior en el rango, se utiliza la primera tasa hábil inmediatamente posterior del mismo país (`2023-01-02`).

## Resultados de ejemplo y análisis de rendimiento

### Extracto del archivo de salida generado (`resumen_ventas_usd.csv`)

Sobre el archivo oficial `data/ventas.csv` (~13,97 GB), el programa generó `resumen_ventas_usd.csv` con **25.357.072 clientes agrupados por país**, ordenados por `pais` y luego por `id_cliente`:

```csv
pais,id_cliente,total_usd
0,0000023f-5079-3946-8aa2-b78875b7e5d6,3942.09
0,0000071c-69e6-3287-8968-27a4173c94c1,5586.30
0,00000989-1526-30ad-a718-0f8423792063,1352.02
0,00000ce8-45c0-3cbf-8686-c992fc369df4,229.67
0,00000f72-64c2-3ba6-bea0-c837ed6aa0aa,1458.62
...
2,fffffa6b-ed6b-3780-8651-49fab05cf406,1592.35
2,fffffcc4-2d1b-37ab-b42a-4fa2d820f9f0,2540.63
2,fffffe98-d096-3d27-815c-198262d97221,2361.64
```

### Benchmark sobre el dataset completo (`data/ventas.csv`, 13,97 GB)

Se evaluó el desempeño del procesamiento paralelo sobre el archivo oficial completo (`data/ventas.csv`, **13,97 GB**) variando el número de hilos OpenMP ($p \in \{1, 2, 4, 8\}$) en un procesador de 8 hilos lógicos y 15 GB de RAM, midiendo cada etapa con `omp_get_wtime()` (datos registrados en [`results/benchmark_tiempos.csv`](results/benchmark_tiempos.csv)):

- **Tiempo de autenticación JWT + 3.288 consultas a la API (en paralelo con HTTP Keep-Alive):** **`36,0490 s`** *(reducido desde `~109 s` al reutilizar las sesiones TLS por hilo)*.

Las métricas evaluadas son:
- **Speedup:** $S(p) = \frac{T(1)}{T(p)}$
- **Eficiencia:** $E(p) = \frac{S(p)}{p}$

| Hilos ($p$) | Tiempo lectura + conversión (s) | Tiempo escritura CSV (s) | Tiempo total procesamiento (s) | Speedup total $S(p)$ | Eficiencia $E(p)$ | Speedup etapa paralela |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 209.0490 | 11.2838 | 220.3327 | 1.0000x | 100.00% | 1.0000x |
| 2 | 127.1123 | 11.9675 | 139.0798 | 1.5842x | 79.21% | 1.6446x |
| 4 | 90.8794 | 12.7161 | 103.5955 | 2.1269x | 53.17% | 2.3003x |
| 8 | 77.6720 | 11.5860 | 89.2580 | 2.4685x | 30.86% | 2.6914x |

### Análisis de resultados

1. **Reducción del tiempo de ejecución:** Al procesar los ~14 GB (~119 millones de ventas), el tiempo total cae de **220,33 s (~3 min 40 s)** con 1 hilo a **89,26 s (~1 min 29 s)** con 8 hilos, logrando un **speedup total de $2,47\times$** (y **$2,69\times$** en la etapa paralela de lectura, validación y conversión).
2. **Cuello de botella de E/S en disco y memoria en un dataset de 14 GB:** A diferencia de problemas puramente sintéticos en caché L1/L2, acá cada pasada lee 13,97 GB desde almacenamiento secundario e inserta/actualiza ~119 millones de boletas y ~25,35 millones de clientes en las tablas hash en RAM. Al pasar de 4 a 8 hilos (que en este procesador corresponden a hilos SMT/Hyper-Threading compartiendo núcleos físicos y controlador de memoria), el ancho de banda de memoria y de lectura de disco se satura, explicando por qué la eficiencia pasa de $79,21\%$ (2 hilos) y $53,17\%$ (4 hilos) a $30,86\%$ (8 hilos).
3. **Ley de Amdahl en la escritura secuencial:** La escritura final de los `25.357.072` registros ordenados en `resumen_ventas_usd.csv` (~1,1 GB de salida) es inherentemente secuencial y toma un tiempo constante de **~11,5–12,7 s** independientemente del número de hilos, acotando el speedup total máximo según la Ley de Amdahl.
4. **Optimización de consultas a la API:** La deduplicación de consultas a los `3.288` pares `(país, fecha)` del período 2023–2025 junto con la descarga paralela con conexiones persistentes (`Keep-Alive`) permite poblar la tabla $O(1)$ una única vez al inicio, evitando realizar 119 millones de peticiones HTTP por red.

## Supuestos

1. El archivo de entrada utiliza codificación ASCII/UTF-8 con saltos de línea `\n` o `\r\n`, separador de columnas `,` (o `;`) y punto `.` como separador decimal.
2. Si la primera línea del archivo no comienza con un dígito ni con el signo `-`, se interpreta como encabezado CSV y se omite automáticamente.
3. Cada monto convertido a USD se redondea a micro-dólares (6 decimales) antes de sumarse; el total se escribe redondeado a 2 decimales. El error de redondeo es menor a una millonésima de dólar por venta.
4. El `id_cliente` se escribe en minúsculas en la salida (los UUID del archivo ya vienen en minúsculas).
