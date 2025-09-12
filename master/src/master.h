#ifndef MASTER_H_
#define MASTER_H_

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
#include "sync.h"

#include "manejo_query.h"
#include "worker_conexion.h"
#include "worker_manager.h"

t_log* logger;
t_config* config;
t_log_level log_level;

void* manejar_conexiones_entrantes(void* arg);


#endif /* MASTER_H_ */