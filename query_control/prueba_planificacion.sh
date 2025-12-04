#!/bin/bash

BIN_QUERY="./bin/query_control"
CONF_QUERY="query.config"


echo "~~~   INICIANDO FASE 1: PRUEBAS FIFO   ~~~"


$BIN_QUERY $CONF_QUERY "FIFO_1" 4 &
echo "-> Lanzado FIFO_1 (Prioridad 4)"

$BIN_QUERY $CONF_QUERY "FIFO_2" 3 &
echo "-> Lanzado FIFO_2 (Prioridad 3)"

$BIN_QUERY $CONF_QUERY "FIFO_3" 5 &
echo "-> Lanzado FIFO_3 (Prioridad 5)"

$BIN_QUERY $CONF_QUERY "FIFO_4" 1 &
echo "-> Lanzado FIFO_4 (Prioridad 1)"

echo ""
echo "Esperando a que terminen las queries FIFO..."

wait

echo ""
echo "  FASE 1 (FIFO) FINALIZADA."
echo "---------------------------------------------------"


read -p "Presiona [ENTER] para lanzar la FASE 2 (AGING)... " input
echo "---------------------------------------------------"

echo ""
echo "~~~    INICIANDO FASE 2: PRUEBAS AGING    ~~~"

$BIN_QUERY $CONF_QUERY "AGING_1" 4 &
echo "-> Lanzado AGING_1 (Prioridad 4)"

$BIN_QUERY $CONF_QUERY "AGING_2" 3 &
echo "-> Lanzado AGING_2 (Prioridad 3)"

$BIN_QUERY $CONF_QUERY "AGING_3" 5 &
echo "-> Lanzado AGING_3 (Prioridad 5)"

$BIN_QUERY $CONF_QUERY "AGING_4" 1 &
echo "-> Lanzado AGING_4 (Prioridad 1)"

echo ""
echo "Esperando a que terminen las queries AGING..."

wait

echo ""
echo "  PRUEBA PLANIFICACION FINALIZADA."