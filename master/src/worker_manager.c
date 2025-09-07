#include "worker_manager.h"

t_list* workers_conectados;

void iniciar_worker_manager() {
    workers_conectados = list_create();
}

