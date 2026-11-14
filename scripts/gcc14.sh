#!/usr/bin/env bash
# Ejecuta un comando dentro del contenedor con GCC 14.2.0, montando el repositorio en /work.
# Uso: scripts/gcc14.sh make
#      scripts/gcc14.sh ./programa --input data/ventas.csv --threads 8
#      scripts/gcc14.sh            (abre una terminal dentro del contenedor)
set -euo pipefail
IMAGEN=cpyd-gcc14
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
MOTOR="$(command -v podman || command -v docker)"

if ! "$MOTOR" image exists "$IMAGEN" 2>/dev/null && ! "$MOTOR" image inspect "$IMAGEN" >/dev/null 2>&1; then
  "$MOTOR" build -t "$IMAGEN" -f "$RAIZ/Containerfile" "$RAIZ"
fi

# Podman y Docker usan opciones distintas para que los archivos creados queden a nombre del usuario.
if [[ "$(basename "$MOTOR")" == podman ]]; then
  USER_ARGS=(--userns=keep-id)
else
  USER_ARGS=(--user "$(id -u):$(id -g)")
fi

ENV_ARGS=()
[[ -f "$RAIZ/.env" ]] && ENV_ARGS=(--env-file "$RAIZ/.env")

TTY=(); [[ -t 0 && -t 1 ]] && TTY=(-it)
exec "$MOTOR" run --rm "${TTY[@]}" "${USER_ARGS[@]}" -v "$RAIZ":/work:Z -w /work "${ENV_ARGS[@]}" "$IMAGEN" "${@:-bash}"
