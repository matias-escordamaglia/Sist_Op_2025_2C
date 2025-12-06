#!/bin/bash

# --- CONFIGURACIÓN ---
BIN_QUERY="./bin/query_control"
CONF_QUERY="query.config"
PATH_QUERY_AGING_1="AGING_1"
PATH_QUERY_AGING_2="AGING_2"
PATH_QUERY_AGING_3="AGING_3"
PATH_QUERY_AGING_4="AGING_4"
PRIORIDAD=20


echo "~~~ LANZANDO 25 INSTANCIAS DE CADA QUERY  (⊙ _ ⊙ ) ~~~"

for i in {1..25}
do
    # se puede agregar "> /dev/null" para ver el caos de logs
    $BIN_QUERY $CONF_QUERY $PATH_QUERY_AGING_1 $PRIORIDAD &
    # se puede quitar el "echo -n "." " dado que solo indica salto
    echo -n "." 

    $BIN_QUERY $CONF_QUERY $PATH_QUERY_AGING_2 $PRIORIDAD &
    echo -n "." 

    $BIN_QUERY $CONF_QUERY $PATH_QUERY_AGING_3 $PRIORIDAD &
    echo -n "." 

    $BIN_QUERY $CONF_QUERY $PATH_QUERY_AGING_4 $PRIORIDAD &
    echo -n "." 

done
echo ""
echo "-> ¡100 Queries lanzadas!"


echo "El script esperará a que terminen TODAS las queries."
echo "Se puede presionar CTRL+C para forzar la salida si se cuelgan."

wait

echo "Prueba finalizada."