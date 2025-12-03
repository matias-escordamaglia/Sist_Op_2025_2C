#!/bin/bash

NUEVA_IP_MASTER=$1
NUEVA_IP_STORAGE=$2


if [ -z "$NUEVA_IP_MASTER" ] || [ -z "$NUEVA_IP_STORAGE" ]; then
    echo "Error: Faltan argumentos."
    echo "Uso correcto: ./configurar_worker.sh <IP_MASTER> <IP_STORAGE>"
    echo "Ejemplo: ./configurar_worker.sh 127.0.0.1 127.0.0.2"
    exit 1
fi

echo "Iniciando configuración de Workers..."


sed -i "s/^IP_MASTER=.*/IP_MASTER=$NUEVA_IP_MASTER/" *.config
echo " -> IP_MASTER actualizada a: $NUEVA_IP_MASTER"


sed -i "s/^IP_STORAGE=.*/IP_STORAGE=$NUEVA_IP_STORAGE/" *.config
echo " -> IP_STORAGE actualizado a: $NUEVA_IP_STORAGE"

echo "¡Configuración finalizada con éxito!"