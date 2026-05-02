#!/usr/bin/env bash
# start.sh — Arranca todos los servicios del sistema en terminales separadas
# Uso: ./start.sh
# Requiere MSYS2 con mintty instalado

BIN="./bin"

echo "[start] Arrancando gesfich..."
start mintty -t "gesfich" -e "$BIN/gesfich.exe" -x "aralmac/ficheros"

echo "[start] Arrancando gesprog..."
start mintty -t "gesprog" -e "$BIN/gesprog.exe" -x "aralmac/programas"

echo "[start] Arrancando ejecutor..."
start mintty -t "ejecutor" -e "$BIN/ejecutor.exe" -x "aralmac/ficheros"

# Dar tiempo a que los servicios estén listos
sleep 1

echo "[start] Arrancando ctrllt..."
start mintty -t "ctrllt" -e "$BIN/ctrllt.exe"

echo "[start] Sistema iniciado."
echo "Usa: $BIN/cliente.exe <servicio> <operacion> [params]"
