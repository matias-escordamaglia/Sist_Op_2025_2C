#!/bin/bash

BIN_WORKER="./bin/worker"
CONF_WORKER1="worker_estabilidad_general1.config"
CONF_WORKER2="worker_estabilidad_general2.config"

echo "~~~   INICIANDO LAS PRUEBAS DE ESTABILIDAD PARTE 1  ~~~"


$BIN_WORKER $CONF_WORKER1 1 &
echo "-> Lanzado Worker 1"

$BIN_WORKER $CONF_WORKER2 2 &
echo "-> Lanzado Worker 2"


echo ""
read -p "Presione enter para finalizar los workers..." input


echo ""
echo "  PRUEBA DE ESTABILIDAD FINALIZADA."