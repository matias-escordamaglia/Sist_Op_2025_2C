#!/bin/bash


BIN_WORKER="./bin/worker"


echo "~~~  PRUEBA DE ESTABILIDAD EN WORKER  ~~~"


echo "Iniciando Workers 1 y 2..."

$BIN_WORKER "worker_estabilidad_general1.config" 1 &
PID_W1=$!
echo "Worker 1 iniciado"

$BIN_WORKER "worker_estabilidad_general2.config" 2 &
PID_W2=$! 
echo "Worker 2 iniciado"



read -p "Presiona enter para lanzar Worker 3 y 4 " input
echo "---------------------------------------------------"



$BIN_WORKER "worker_estabilidad_general3.config" 3 &
echo "Worker 3 iniciado"

$BIN_WORKER "worker_estabilidad_general4.config" 4 &
echo "Worker 4 iniciado"



read -p "Presiona enter para finalizar Worker 1 y 2 " input
echo "---------------------------------------------------"



kill -9 $PID_W1
echo "Worker 1 terminado."

kill -9 $PID_W2
echo "Worker 2 terminado."


read -p "Presiona enter para lanzar Worker 5 y 6 " input
echo "---------------------------------------------------"



$BIN_WORKER "worker_estabilidad_general5.config" 5 &
echo "Worker 5 iniciado"

$BIN_WORKER "worker_estabilidad_general6.config" 6 &
echo "Worker 6 iniciado"

echo "Se lanzaron todos los workers. Cerrar script con ctrl+c una vez finalicen"

wait