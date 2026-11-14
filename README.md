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

El enunciado exige que compile en **GCC 14.2.0**. Para que todo el grupo use exactamente esa versión, sin importar la
que traiga su sistema, se compila dentro de un contenedor basado en la imagen oficial `gcc:14.2.0`
([`Containerfile`](Containerfile)), que ya incluye g++, OpenMP y libcurl.

- Podman (viene en Fedora) o Docker
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, incluido en `third_party/json.hpp`

```bash
scripts/gcc14.sh gcc --version     # la primera vez construye la imagen; debe decir 14.2.0
scripts/gcc14.sh                   # abre una terminal dentro del contenedor
```

## Credenciales

El token JWT **no** se guarda en el repositorio: el programa hace login al iniciar con el correo UTEM y el RUT.

```bash
cp .env.example .env      # .env está en .gitignore
nano .env                 # completar CPYD_EMAIL y CPYD_APIKEY (RUT con puntos y guion)
```

## Compilación y ejecución

```bash
scripts/gcc14.sh make
scripts/gcc14.sh ./programa --input data/ventas.csv --output resumen_ventas_usd.csv --threads 8
```

`scripts/gcc14.sh` carga automáticamente el `.env` dentro del contenedor.

| Opción | Descripción | Por defecto |
|---|---|---|
| `--input` | archivo de ventas | obligatorio |
| `--output` | CSV de salida `pais,id_cliente,total_usd` | `resumen_ventas_usd.csv` |
| `--errors` | filas rechazadas con el motivo | `errores.csv` |
| `--threads` | número de hilos OpenMP | lo que defina OpenMP |

## Flujo de trabajo (Gitflow)

| Rama | Para qué | Sale de | Se une a |
|---|---|---|---|
| `main` | Solo versiones entregables, cada una con su tag (`v1.0.0`) | — | — |
| `develop` | Integración del trabajo del grupo (rama por defecto) | `main` | — |
| `feature/<tema>` | Una funcionalidad: `feature/lectura-csv`, `feature/cliente-api` | `develop` | `develop` |
| `release/<versión>` | Preparar una entrega: README, resultados, ajustes finales | `develop` | `main` y `develop` |
| `hotfix/<versión>` | Corrección urgente sobre lo ya entregado | `main` | `main` y `develop` |

```bash
# empezar una funcionalidad
git switch develop && git pull
git switch -c feature/lectura-csv
# ... commits ...
git push -u origin feature/lectura-csv     # y abrir un pull request hacia develop

# preparar la entrega
git switch -c release/1.0.0 develop
# ... últimos ajustes ...
git switch main && git merge --no-ff release/1.0.0 && git tag -a v1.0.0 -m "Entrega Tarea 1"
git switch develop && git merge --no-ff release/1.0.0
git push origin main develop --tags
```

Mensajes de commit: `tipo: descripción` en español, en imperativo (`feat: lee y valida el archivo de ventas`,
`fix: corrige fechas sin tasa`, `docs: agrega análisis de speedup`, `perf: …`, `test: …`, `chore: …`).
Cada integrante hace commits **desde su propia cuenta** (requisito de participación verificable).

## Formato de entrada

_Por confirmar con el archivo oficial._ Campos por línea, en orden (según el enunciado):

- `pais`: 0 = Chile, 1 = Colombia, 2 = Ecuador
- `id_cliente`: UUID
- `id_boleta`: UUID, único en todo el archivo
- fecha y hora local de la venta
- `monto`: decimal, en moneda local

## Estrategia de paralelización

_Por completar._ Ver la propuesta en [`docs/PANORAMA.md`](docs/PANORAMA.md#5-estrategia-de-paralelización-lo-que-más-pesa-en-la-nota).

## Política de errores

_Por completar:_ filas inválidas, boletas duplicadas, errores HTTP y de token, fechas sin tasa.

## Resultados y análisis de rendimiento

_Por completar:_ tiempos con 1, 2, 4, 8 y 16 hilos, speedup y eficiencia.

## Supuestos

_Por completar._
