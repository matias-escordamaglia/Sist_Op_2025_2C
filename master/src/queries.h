#ifndef QUERIES_H_
#define QUERIES_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

#include "./utils/utils.h"
#include "sync.h"
#include "planificacion.h"



t_query* crear_query(char* query_path, uint32_t prioridad, int conexion);
uint32_t establecer_siguiente_valor_qid();
t_query* obtener_query_por_id_uso_externo(uint32_t id_query);
t_query* obtener_query_por_id_uso_interno(uint32_t id_query);
int conexion_de_query_por_id(uint32_t id_query);



#endif /* QUERIES_H_ */