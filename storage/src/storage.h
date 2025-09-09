#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"
#include "utils-storage.h"

#include "manejo-worker.h"

//configs
char* PUERTO_ESCUCHA; 
bool FRESH_START;
char* PUNTO_MONTAJE;
int RETARDO_OPERACION;
int RETARDO_ACCESO_BLOQUE; 


t_log_level log_level;
void extraer_storage_config(t_config* config);



#endif /* STORAGE_H_ */