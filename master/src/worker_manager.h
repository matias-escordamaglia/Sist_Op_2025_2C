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


typedef struct {
    uint32_t id_worker;
    int fd_dworker;
    bool worker_conectado;
    int qid_actual;
} t_worker_conectado;


void iniciar_worker_manager(); 

void registrar_worker(uint32_t id_worker, int cliente_fd);
t_worker_conectado* obtener_worker_por_id_uso_externo(uint32_t id_worker);
t_worker_conectado* obtener_worker_por_id_uso_interno(uint32_t id_worker);
t_worker_conectado* obtener_worker_libre();
void establecer_worker_desalojado(uint32_t id_worker);
void remover_cpu(t_worker_conectado* worker);


#endif /* WORKER_MANAGER_H_ */