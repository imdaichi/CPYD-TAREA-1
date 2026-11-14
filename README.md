# CPYD Tarea 1: ventas multinacionales a USD con OpenMP

Computación Paralela y Distribuida, UTEM. Profesor: Sebastián Salazar Molina.
Enunciado: [`docs/enunciado/enunciado.pdf`](docs/enunciado/enunciado.pdf). Resumen y plan: [`docs/PANORAMA.md`](docs/PANORAMA.md).

Programa en C++17 con OpenMP que lee ventas de Chile, Colombia y Ecuador, filtra las del 01/01/2023 al 31/12/2025,
convierte cada monto a USD con la tasa diaria de la API del curso y escribe el total por cliente y país.

## Integrantes

| Nombre | GitHub | Aporte |
|---|---|---|
| Renato Moyano | `imdaichi` | _por completar_ |
| _por completar_ | | |
| _por completar_ | | |

## Requisitos

- GCC con soporte OpenMP (el enunciado pide que compile en **GCC 14.2.0**) y `make`
- libcurl con cabeceras de desarrollo
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, incluido en `third_party/json.hpp`

```bash
# Fedora
sudo dnf install gcc-c++ make libcurl-devel
# Debian / Ubuntu
sudo apt install g++ make libcurl4-openssl-dev
```

## Credenciales

El token JWT **no** se guarda en el repositorio: el programa hace login al iniciar con el correo UTEM y el RUT.

```bash
cp .env.example .env      # .env está en .gitignore
nano .env                 # completar CPYD_EMAIL y CPYD_APIKEY (RUT con puntos y guion)
set -a; source .env; set +a
```

## Compilación y ejecución

```bash
make
./programa --input data/ventas.csv --output resumen_ventas_usd.csv --threads 8
```

| Opción | Descripción | Por defecto |
|---|---|---|
| `--input` | archivo de ventas | obligatorio |
| `--output` | CSV de salida `pais,id_cliente,total_usd` | `resumen_ventas_usd.csv` |
| `--errors` | filas rechazadas con el motivo | `errores.csv` |
| `--threads` | número de hilos OpenMP | lo que defina OpenMP |

## Formato de entrada

_Por confirmar con el archivo oficial._ Formato asumido:

```
pais,id_cliente,id_boleta,fecha_hora,monto
0,3eabedcb-baa8-4dd4-88bd-64072bcfbe01,d7c524a5-5304-417f-af42-e12f3838b326,2023-01-23 08:20:21,15990.00
```

- `pais`: 0 = Chile, 1 = Colombia, 2 = Ecuador
- `id_cliente`, `id_boleta`: UUID
- `fecha_hora`: `YYYY-MM-DD HH:MM:SS`
- `monto`: decimal con punto, en moneda local

Para probar sin el archivo oficial: `python3 scripts/generar_ventas.py --rows 100000 --out data/ventas_prueba.csv`
(incluye filas inválidas a propósito).

## Estrategia de paralelización

_Por completar._ Ver la propuesta en [`docs/PANORAMA.md`](docs/PANORAMA.md#5-estrategia-de-paralelización-lo-que-más-pesa-en-la-nota).

## Política de errores

_Por completar:_ filas inválidas, boletas duplicadas, errores HTTP y de token, fechas sin tasa.

## Resultados y análisis de rendimiento

_Por completar:_ tiempos con 1, 2, 4, 8 y 16 hilos, speedup y eficiencia.

## Supuestos

_Por completar._
