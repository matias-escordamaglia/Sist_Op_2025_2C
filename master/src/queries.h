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



t_query* crear_query(char* query_path, uint32_t prioridad);
uint32_t establecer_siguiente_valor_qid();



#endif /* QUERIES_H_ */