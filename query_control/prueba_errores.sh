#!/bin/bash

BIN_QUERY="./bin/query_control"
CONF_QUERY="query.config"


echo "~~~   INICIANDO LAS PRUEBAS DE ERROR   ~~~"


$BIN_QUERY $CONF_QUERY "ESTRUCTURA_ARCHIVO_COMMITED" 1 &
echo "-> Lanzado ESTRUCTURA_ARCHIVO_COMMITED"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "FILE_EXISTENTE" 1 &
echo "-> Lanzado FILE_EXISTENTE"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "LECTURA_FUERA_DE_LIMITE" 1 &
echo "-> Lanzado LECTURA_FUERA_DE_LIMITE"

wait
read -p "Presionar ener para ejecutar el siguiente" input

$BIN_QUERY $CONF_QUERY "TAG_EXISTENTE" 1 &
echo "-> Lanzado TAG_EXISTENTE"

echo ""
echo "Esperando a que terminen las queries..."

wait

echo ""
echo "  PRUEBA ERRORES FINALIZADA."