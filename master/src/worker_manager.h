#ifndef WORKER_MANAGER_H_
#define WORKER_MANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/queue.h>
#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "sync.h"


typedef struct {
    uint32_t id_worker;
    int fd_worker;
    bool worker_conectado;
    int qid_actual;
} t_worker_conectado;


typedef struct {
    uint32_t qid;
    uint32_t pc;
    char* query_path;
    t_worker_conectado* worker_asignado;
} t_siguiente_pedido;

void iniciar_worker_manager(); 

void registrar_worker(uint32_t id_worker, int cliente_fd);
t_worker_conectado* obtener_worker_por_id_uso_externo(uint32_t id_worker);
t_worker_conectado* obtener_worker_por_id_uso_interno(uint32_t id_worker);
t_worker_conectado* obtener_worker_libre();
void establecer_worker_desalojado(uint32_t id_worker);
void remover_cpu(t_worker_conectado* worker);

void agregar_siguiente_proceso_a_enviar(t_query* query, t_worker_conectado* worker_libre);
bool enviar_siguiente_query(t_worker_conectado* worker, t_pedido_master_worker* sig_pedido);
void* tratar_siguientes_queries_a_enviar(void* _);


#endif /* WORKER_MANAGER_H_ */