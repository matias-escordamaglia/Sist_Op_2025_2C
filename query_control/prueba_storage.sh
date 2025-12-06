#!/bin/bash

BIN_QUERY="./bin/query_control"
CONF_QUERY="query.config"


echo "~~~   INICIANDO LAS PRUEBAS DE STORAGE   ~~~"


$BIN_QUERY $CONF_QUERY "STORAGE_1" 1 &
echo "-> Lanzado STORAGE_1"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "STORAGE_2" 1 &
echo "-> Lanzado STORAGE_2"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "STORAGE_3" 1 &
echo "-> Lanzado STORAGE_3"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "STORAGE_4" 1 &
echo "-> Lanzado STORAGE_4"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "STORAGE_5" 1 &
echo "-> Lanzado STORAGE_5"

echo ""
echo "Esperando a que terminen las queries..."

wait

echo ""
echo "  PRUEBA DE STORAGE FINALIZADA."