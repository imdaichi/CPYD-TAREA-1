#!/usr/bin/env bash
# Ejecuta el benchmark con 1, 2, 4 y 8 hilos usando GCC 14.2.0 y verifica consistencia de salidas.
# Uso: ./scripts/benchmark.sh [ruta_csv]
set -euo pipefail

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
ARCHIVO_ENTRADA="${1:-data/ventas.csv}"
SALIDA_BENCH="$RAIZ/results/benchmark_tiempos.csv"

cd "$RAIZ"
./scripts/gcc14.sh make

echo "=== Ejecutando benchmark integrado (1, 2, 4 y 8 hilos) sobre $ARCHIVO_ENTRADA ==="
./scripts/gcc14.sh ./programa --input "$ARCHIVO_ENTRADA" --output resumen_ventas_usd.csv --threads 8 --benchmark

echo ""
echo "=== Verificando ausencia de condiciones de carrera (comparando salida de 1 hilo vs 8 hilos) ==="
./scripts/gcc14.sh ./programa --input "$ARCHIVO_ENTRADA" --output resumen_1hilo.csv --threads 1
./scripts/gcc14.sh ./programa --input "$ARCHIVO_ENTRADA" --output resumen_8hilos.csv --threads 8

if diff -q resumen_1hilo.csv resumen_8hilos.csv >/dev/null; then
    echo "[OK] Los archivos resumen_1hilo.csv y resumen_8hilos.csv son IDENTICOS."
else
    echo "[ERROR] Se detectaron diferencias entre las salidas de 1 y 8 hilos." >&2
    rm -f resumen_1hilo.csv resumen_8hilos.csv
    exit 1
fi

rm -f resumen_1hilo.csv resumen_8hilos.csv
