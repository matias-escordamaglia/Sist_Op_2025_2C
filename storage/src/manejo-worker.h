#ifndef MANEJO_WORKER_H_
#define MANEJO_WORKER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"

extern t_config* blockconfig;

void pasar_log_config_a_manejo_worker(t_log* l, t_config*);
void* manejar_cliente_worker(void* arg);
void* atender_conexion_worker(void* arg);



#endif /* MANEJO_WORKER_H_ */