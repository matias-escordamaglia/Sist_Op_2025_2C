#ifndef WORKER_CONEXION_H_
#define WORKER_CONEXION_H_

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
#include "worker_manager.h"


void* manejar_worker(void* arg);


#endif /* WORKER_CONEXION_H_ */