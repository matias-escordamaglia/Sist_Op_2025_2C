#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "sync.h"
#include "queries.h"

typedef struct 
{
    t_query* query;
    uint64_t tiempo_llegada;
}t_elemento_cola;


void inicializar_listas_planificacion();
uint64_t timestamp_actual_en_milisegundos();
void crear_nuevo_query(char* query_path, uint32_t prioridad);
t_elemento_cola* crear_nuevo_elemento(t_query* query);
t_elemento_cola* buscar_y_remover_por_qid(t_list* lista, uint32_t qid);
void *iniciador_planificacion();
void planificar_por_fifo();
void planificar_por_prioridades();



#endif /* PLANIFICACION_H_ */