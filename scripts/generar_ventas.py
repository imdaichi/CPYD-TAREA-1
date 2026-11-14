#!/usr/bin/env python3
"""Genera un archivo de ventas de PRUEBA mientras no se tenga el archivo oficial.

Formato asumido (ajustar cuando llegue el archivo real):
    pais,id_cliente,id_boleta,fecha_hora,monto
    0,<uuid>,<uuid>,2024-03-15 14:22:05,15990.00

Incluye a propósito filas inválidas para probar las validaciones.
Uso: python3 scripts/generar_ventas.py --rows 100000 --clients 2000 --out data/ventas_prueba.csv
"""
import argparse
import random
import uuid
from datetime import datetime, timedelta

RANGOS_MONTO = {0: (1000, 500000), 1: (5000, 2000000), 2: (1, 500)}  # CLP, COP, USD aprox.


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rows", type=int, default=100000)
    ap.add_argument("--clients", type=int, default=2000, help="clientes por país")
    ap.add_argument("--bad", type=float, default=0.01, help="fracción de filas inválidas")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--out", default="data/ventas_prueba.csv")
    a = ap.parse_args()

    rnd = random.Random(a.seed)
    clientes = {p: [str(uuid.UUID(int=rnd.getrandbits(128), version=4)) for _ in range(a.clients)] for p in range(3)}
    # algunos clientes repetidos entre países (el enunciado lo permite)
    for i in range(min(50, a.clients)):
        clientes[1][i] = clientes[0][i]

    ini = datetime(2022, 10, 1)
    dias = (datetime(2026, 3, 31) - ini).days  # incluye fechas fuera de rango
    boletas = []
    with open(a.out, "w", encoding="utf-8") as f:
        f.write("pais,id_cliente,id_boleta,fecha_hora,monto\n")
        for _ in range(a.rows):
            p = rnd.randrange(3)
            cli = rnd.choice(clientes[p])
            bol = str(uuid.UUID(int=rnd.getrandbits(128), version=4))
            fh = ini + timedelta(days=rnd.randrange(dias), seconds=rnd.randrange(86400))
            lo, hi = RANGOS_MONTO[p]
            monto = f"{rnd.uniform(lo, hi):.2f}"
            fecha = fh.strftime("%Y-%m-%d %H:%M:%S")
            if rnd.random() < a.bad:
                tipo = rnd.randrange(6)
                if tipo == 0: p = rnd.choice([3, -1, "x"])
                elif tipo == 1: cli = "no-es-un-uuid"
                elif tipo == 2 and boletas: bol = rnd.choice(boletas)  # boleta duplicada
                elif tipo == 3: monto = rnd.choice(["-150.00", "0", "abc"])
                elif tipo == 4: fecha = fh.strftime("%d/%m/%Y %H:%M")  # formato inconsistente
                else: fecha = "2024-02-30 10:00:00"  # fecha imposible
            boletas.append(bol)
            f.write(f"{p},{cli},{bol},{fecha},{monto}\n")
    print(f"Escrito {a.out} con {a.rows} filas")


if __name__ == "__main__":
    main()
