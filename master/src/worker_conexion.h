#ifndef WORKER_CONEXION_H_
#define WORKER_CONEXION_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "sync.h"
#include "worker_manager.h"
#include "manejo_query.h"


void* manejar_worker(void* arg);
void alta_aviso_confirmacion(t_motivo_pedido_master_worker motivo_pedido, uint32_t query_id, uint32_t id_worker);

void agregar_siguiente_query_a_enviar(t_query* query, t_worker_conectado* worker_libre, t_confirmacion_pedido* conf);
void agregar_pedido_interrupcion(t_worker_conectado* worker, uint32_t query_id, t_confirmacion_pedido* conf);
bool enviar_siguiente_pedido(t_worker_conectado* worker, t_pedido_master_worker* sig_pedido);
void* tratar_siguientes_pedidos_a_enviar_worker(void* _);
bool asignar_query_a_worker(t_query* query, t_worker_conectado* worker);

void inicializar_sistema_confirmaciones();


#endif /* WORKER_CONEXION_H_ */