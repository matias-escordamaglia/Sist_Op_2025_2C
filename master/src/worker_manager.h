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
#include "sync.h"


typedef struct {
    uint32_t id_worker;
    int fd_worker;
    bool worker_conectado;
    int qid_actual;
} t_worker_conectado;

typedef struct {
    sem_t sem_respuesta;
    bool respuesta_recibida;
    uint32_t worker_id;
    uint32_t query_id;
    t_motivo_pedido_master_worker tipo_pedido;
} t_confirmacion_pedido;

typedef struct {
    uint32_t qid;
    uint32_t pc;
    char* query_path;
    t_worker_conectado* worker_asignado;
    t_motivo_pedido_master_worker tipo;
    t_confirmacion_pedido* confirmacion;
} t_siguiente_pedido;

void iniciar_worker_manager(); 

void registrar_worker(uint32_t id_worker, int cliente_fd);
t_worker_conectado* obtener_worker_por_id_uso_externo(uint32_t id_worker);
t_worker_conectado* obtener_worker_por_id_uso_interno(uint32_t id_worker);
t_worker_conectado* obtener_worker_por_query_id(uint32_t query_id);
t_worker_conectado* obtener_worker_libre();
uint32_t get_worker_qid(uint32_t id_worker);
void establecer_worker_desalojado(uint32_t id_worker);
void marcar_worker_desconectado(uint32_t id_worker);
void marcar_worker_conectado(uint32_t id_worker);
void remover_cpu(t_worker_conectado* worker);
int get_cant_workers_conectados();
void asociar_qid_a_worker(uint32_t qid, t_worker_conectado* worker);

#endif /* WORKER_MANAGER_H_ */