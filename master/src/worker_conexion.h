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
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "sync.h"
#include "worker_manager.h"
#include "manejo_query.h"


void* manejar_worker(void* arg);


void agregar_siguiente_query_a_enviar(t_query* query, t_worker_conectado* worker_libre);
bool enviar_siguiente_query(t_worker_conectado* worker, t_pedido_master_worker* sig_pedido);
void* tratar_siguientes_queries_a_enviar(void* _);


#endif /* WORKER_CONEXION_H_ */