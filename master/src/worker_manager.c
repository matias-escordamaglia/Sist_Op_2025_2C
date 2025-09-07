#include "worker_manager.h"

#define QID_NULO -1

t_list* workers_conectados;

pthread_mutex_t mutex_workers_conectados = PTHREAD_MUTEX_INITIALIZER;

void iniciar_worker_manager() {
    workers_conectados = list_create();
}

void registrar_worker(uint32_t id_worker, int cliente_fd) {
    t_worker_conectado* worker;

    worker = malloc(sizeof(t_worker_conectado));
    worker -> qid_actual = QID_NULO;
    worker -> fd_dworker = cliente_fd;
    worker -> id_worker = id_worker;
    worker -> worker_conectado = true;

    pthread_mutex_lock(&mutex_workers_conectados);

    list_add(workers_conectados, worker);

    pthread_mutex_unlock(&mutex_workers_conectados);
 }

t_worker_conectado* obtener_worker_por_id_uso_externo(uint32_t id_worker) {
    pthread_mutex_lock(&mutex_workers_conectados);
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_conectados); i++) {
        t_worker_conectado* worker = list_get(workers_conectados, i);
        if (worker->id_worker == id_worker) {
            encontrado = worker;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_workers_conectados);
    return encontrado;
}

