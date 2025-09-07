#ifndef WORKER_MANAGER_H_
#define WORKER_MANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


#endif /* WORKER_MANAGER_H_ */