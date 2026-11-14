# Entorno de compilación con la versión exigida por el enunciado: GCC 14.2.0
# (imagen oficial de GCC, base Debian). Incluye gcc, g++ y OpenMP (libgomp).
FROM docker.io/library/gcc:14.2.0

RUN apt-get update \
 && apt-get install -y --no-install-recommends libcurl4-openssl-dev make \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
