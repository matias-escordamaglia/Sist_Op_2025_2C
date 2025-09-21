#ifndef MANEJO_QUERY_H_
#define MANEJO_QUERY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "sync.h"
#include "queries.h"

void* manejar_query(void* arg);


bool mandar_lectura_a_query_con_id(char* string_crudo, uint32_t id_query);
bool separar_string(char* input, char** file_tag, char** lectura);



#endif /* MANEJO_QUERY_H_ */