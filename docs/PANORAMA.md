# Panorama: Tarea OpenMP, ventas multinacionales a USD

Ramo: Computación Paralela y Distribuida (UTEM). Profesor: Sebastián Salazar Molina.
Enunciado: [`material-curso/enunciado.pdf`](material-curso/enunciado.pdf) (también en [`.md`](material-curso/enunciado.md)).

> ⚠️ **Plazo: sábado 14/11/2026 a las 12:30 (hora de Chile).** El enunciado tiene fecha 14/11 y el plazo es el mismo día.
> Hay que agregar a **`sebasalazar`** como colaborador del repositorio de entrega.

---

## 1. Qué hay que hacer, en una frase

Un programa en C/C++ con OpenMP que lee un archivo de ventas de Chile, Colombia y Ecuador, se queda con las ventas
válidas entre el 01/01/2023 y el 31/12/2025, convierte cada monto a USD con la tasa del día que entrega una API con JWT
y escribe `pais,id_cliente,total_usd`, con el total por cliente y país, ordenado.

```
./programa --input ventas.csv --output resumen_ventas_usd.csv --threads 8
```

## 2. Flujo del programa

```
 ventas.csv ──► [1] Leer y validar ──► errores.csv (filas inválidas / boletas duplicadas)
                     │
                     ▼
               [2] Conjunto de pares únicos (país, fecha)      ← máx. 3 países × 1.096 días ≈ 3.288
                     │
   login (JWT) ──► [3] Descargar tasas en paralelo (OpenMP) ──► caché (país, fecha) → rate
                     │        + política para fechas sin tasa
                     ▼
               [4] Convertir y acumular en paralelo (mapas por hilo + mezcla)
                     │
                     ▼
               [5] Ordenar por país, luego id_cliente ──► resumen_ventas_usd.csv
```

**La idea clave de rendimiento:** no se llama a la API una vez por venta, sino **una vez por par (país, fecha)
distinto**. Aunque el archivo tenga millones de filas, son a lo más unas 3.288 consultas. Esa deduplicación es la
"optimización" que pide el requisito 5, y las consultas HTTP (que esperan red) son lo que más se beneficia de
paralelizar.

## 3. Datos de entrada

Una línea por venta, 5 campos en orden:

| Campo | Tipo | Validación |
|---|---|---|
| País | entero | 0 = Chile, 1 = Colombia, 2 = Ecuador |
| ID cliente | UUID | formato UUID `8-4-4-4-12` hex. Se repite entre compras y entre países: la clave es **(país, cliente)** |
| ID boleta | UUID | formato UUID y **única en todo el archivo** |
| Fecha/hora local | fecha y hora | formato consistente, fecha real (sin 30/02) y dentro de 2023-01-01 … 2025-12-31 inclusive |
| Monto | decimal | numérico y > 0, en moneda local |

**Pendiente de confirmar** (no viene en el enunciado): no se adjuntó el archivo `ventas.csv`. Hace falta saber el
separador, si tiene encabezado, el formato exacto de fecha (`2025-06-20 14:32:10`, `20/06/2025 …`, ISO con `T`…) y el
separador decimal. Si el profesor no lo entrega, se genera un archivo de prueba propio y se documenta el formato
asumido en el README.

Política de errores propuesta (hay que explicitarla en el README): la fila inválida se escribe en `errores.csv` con
número de línea y motivo, se cuenta y se ignora. Con boletas duplicadas: se conserva la primera aparición y se rechazan
las siguientes (alternativa: rechazar todas las copias; cualquiera sirve si se documenta).

## 4. La API

| | |
|---|---|
| Login | `POST https://api.sebastian.cl/cpyd/v1/login/authenticate` con `{"email": "<correo UTEM>", "apiKey": "<RUT con puntos y DV>"}` → campo `jwt` |
| Tasa | `GET https://api.sebastian.cl/cpyd/v1/exchanges/{chile|colombia|ecuador}/{YYYY-MM-DD}` con `Authorization: Bearer <jwt>` → `{"country","date","rate"}` |
| Conversión | `monto_usd = monto_local / rate` (rate = moneda local por 1 USD) |

Reglas: el token se pide **una vez al inicio** y se reutiliza; **nada hardcodeado** (ni token, ni tasas, ni
credenciales). Las credenciales se leen de variables de entorno (`CPYD_EMAIL`, `CPYD_APIKEY`) o de un `.env` que esté en
`.gitignore`, con un `.env.example` en el repo.

Errores que hay que manejar:

- **HTTP / timeout:** timeout en curl y reintentos con espera creciente (por ejemplo 3 intentos).
- **401 / token expirado:** volver a hacer login una sola vez (protegido con `#pragma omp critical` para que no lo
  hagan todos los hilos a la vez) y repetir la consulta.
- **JSON inválido o sin `rate`:** se trata como "sin tasa".
- **Fecha sin tasa (404 o vacío):** política propuesta: usar la **última tasa anterior disponible** del mismo país; si
  no hay ninguna anterior, la siguiente posterior. Se registra cada caso en un log. Se resuelve **después** de
  descargar todo, de forma secuencial y ordenada por fecha, para que el resultado no dependa del orden de los hilos.

Librerías: **libcurl** para HTTP y **nlohmann/json** (un solo header) o **cJSON** para el JSON.

## 5. Estrategia de paralelización (lo que más pesa en la nota)

| Etapa | Qué se paraleliza | Cómo se evita la condición de carrera |
|---|---|---|
| Descarga de tasas | `#pragma omp parallel for schedule(dynamic)` sobre el vector de pares (país, fecha) únicos | Cada hilo tiene **su propio `CURL*`**. Cada iteración escribe en **su propia posición** de un vector de resultados: no se comparte nada. `curl_global_init` se llama una vez antes de la región paralela |
| Validación / parseo | `parallel for` sobre las líneas ya leídas en memoria | Cada hilo valida su línea; la unicidad de boletas se comprueba después con un ordenamiento o un `unordered_set` secuencial (o un set por hilo y mezcla) |
| Conversión y suma | `parallel for` sobre las ventas válidas | **Mapa por hilo** `(país, cliente) → suma` y mezcla final en una sección `critical` (o mezcla en árbol). Evita poner un `critical` por venta, que mataría el speedup |
| Salida | Secuencial (ordenar y escribir es barato) | — |

Por qué esta estrategia y no otra (para el informe): la descarga es *I/O-bound* (esperar respuestas de red), así que
con más hilos se solapan las esperas y el speedup puede ser casi lineal aunque haya pocos núcleos. La conversión y la
suma son *CPU-bound* y escalan con los núcleos. `schedule(dynamic)` en la descarga reparte bien porque cada request
tarda distinto.

Detalle de precisión: las sumas en `double` en paralelo pueden diferir en los últimos decimales según el número de
hilos (la suma de punto flotante no es asociativa). Conviene redondear a 2 decimales al escribir y mencionarlo.

## 6. Análisis de rendimiento que hay que entregar

1. Medir con `omp_get_wtime()` cada etapa por separado (login, lectura, descarga, conversión, escritura) y el total.
2. Correr con 1, 2, 4, 8 (y 16) hilos, varias repeticiones, y quedarse con el promedio o la mediana.
3. Tabla y gráfico de **tiempo**, **speedup** `S(p) = T(1) / T(p)` y **eficiencia** `E(p) = S(p) / p`.
4. Comentar la Ley de Amdahl: la parte secuencial (login, lectura del archivo, ordenar y escribir) limita el speedup
   máximo.
5. Opcional pero vistoso: comparar **con caché / deduplicación vs. una llamada por venta** para mostrar el efecto de la
   optimización (con pocas filas, para no saturar la API).

Ojo: si la API limita la cantidad de requests por segundo, demasiados hilos pueden empeorar el tiempo. Eso también es
un hallazgo válido para el análisis.

## 7. Estructura sugerida del repo de entrega

```
tarea-openmp/
├── src/
│   ├── main.cpp           # argumentos (--input --output --threads), orquesta etapas, tiempos
│   ├── csv_reader.*       # lectura y validación, errores.csv
│   ├── api_client.*       # login, GET de tasas, reintentos, re-login
│   ├── rate_cache.*       # pares únicos, descarga paralela, política de fechas sin tasa
│   ├── aggregator.*       # conversión y acumulación paralela
│   └── output.*           # orden y escritura del CSV
├── third_party/json.hpp   # nlohmann/json
├── data/                  # ventas de ejemplo (si no son confidenciales)
├── results/               # tiempos, gráficos de speedup
├── scripts/benchmark.sh   # corre con 1..N hilos
├── Makefile               # g++ -std=c++17 -O2 -fopenmp ... -lcurl
├── .env.example
├── .gitignore             # .env, binarios
└── README.md
```

## 8. Checklist de entregables (README.md)

- [ ] Código fuente C/C++ que compile con **GCC 14.2.0** y `-fopenmp`
- [ ] Instrucciones de compilación y ejecución (incluida la dependencia `libcurl4-openssl-dev`)
- [ ] Descripción del formato de entrada usado
- [ ] Estrategia de paralelización (sección 5)
- [ ] Política de errores, boletas duplicadas y fechas sin tasa
- [ ] Resultados de ejemplo y análisis de rendimiento (tablas y gráficos de speedup y eficiencia)
- [ ] Cómo configurar credenciales sin exponer el token (`.env.example`)
- [ ] Registro de participación del grupo (quién hizo qué, y commits de **todos** los integrantes)
- [ ] `sebasalazar` agregado como colaborador del repo
- [ ] Supuestos adicionales documentados

## 9. Plan de trabajo para llegar a las 12:30

| Bloque | Tarea |
|---|---|
| 1 | Conseguir `ventas.csv` (o generar uno), confirmar el formato de fecha y probar login + una consulta con `curl` en la terminal |
| 2 | Lectura, validación y `errores.csv` (secuencial primero) |
| 3 | Cliente API + caché de tasas, primero secuencial, después paralelo |
| 4 | Conversión y acumulación con mapas por hilo; escritura ordenada |
| 5 | Verificar que el resultado con 1 hilo y con 8 hilos sea idéntico (prueba de que no hay carreras) |
| 6 | Benchmark, gráficos, README |
| 7 | Repartir commits entre integrantes, agregar al profesor, entregar |

## 10. Preguntas abiertas

1. **No tenemos el archivo de ventas.** El enunciado nunca lo adjunta ni le da nombre: dice "se dispone de un archivo
   de transacciones" (Contexto, p. 2) y describe sus campos (Datos de entrada, p. 3). `ventas.csv` aparece solo como
   ejemplo en el comando de la p. 9. Hay que pedírselo al profesor o buscarlo en la plataforma del curso; mientras
   tanto se trabaja con un archivo de prueba generado por nosotros.
   Se revisó el PDF en busca de contenido oculto (texto blanco o diminuto, capas, adjuntos, metadatos, texto bajo
   imágenes, caracteres invisibles) y **no hay nada escondido**: tampoco hay un enlace o pista del archivo de ventas.
2. ~~¿Individual o en grupo?~~ **En grupo** (máx. 3): todos con commits y registro de participación.
3. ~~¿Repo nuevo?~~ **Sí, un repo nuevo y dedicado**, todavía sin crear. Mientras tanto el material vive en esta rama.
4. GCC 14.2.0: el contenedor de esta sesión tiene GCC 13.3. El código debe evitar cosas exclusivas de una versión;
   C++17 compila igual en ambas.
