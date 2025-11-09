#!/bin/bash

# Directorios de cada módulo
STORAGE_DIR="./storage/bin"
MASTER_DIR="./master/bin"
WORKER_DIR="./worker/bin"
QUERY_DIR="./query_control"

# Función para ejecutar un módulo en xterm
run_module() {
    local dir="$1"
    local cmd="$2"

    if command -v xterm &> /dev/null; then
        xterm -hold -e "cd $dir; $cmd" &
    else
        echo "xterm no está instalado. No se puede ejecutar $cmd"
        exit 1
    fi
}

# Ejecutar cada módulo con su configuración (solo nombre de archivo)
run_module "$STORAGE_DIR" "./storage storage.config superblock.config"
run_module "$MASTER_DIR" "./master master.config"
run_module "$WORKER_DIR" "./worker worker.config 1"
run_module "$QUERY_DIR" "./query_control query.config modulo 1"

# Espera a que todos los xterm se cierren
wait

