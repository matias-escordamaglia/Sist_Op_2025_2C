#include "worker_manager.h"

#define QID_NULO -1

t_list* workers_registrados;

pthread_mutex_t mutex_workers_conectados = PTHREAD_MUTEX_INITIALIZER;

void iniciar_worker_manager() {
    workers_registrados = list_create();
}

void registrar_worker(uint32_t id_worker, int cliente_fd) {
    t_worker_conectado* worker;

    worker = malloc(sizeof(t_worker_conectado));
    worker -> qid_actual = QID_NULO;
    worker -> fd_worker = cliente_fd;
    worker -> id_worker = id_worker;
    worker -> worker_conectado = true;

    LOCK(&mutex_workers_conectados);

    list_add(workers_registrados, worker);

    UNLOCK(&mutex_workers_conectados);
}

t_worker_conectado* obtener_worker_por_id_uso_externo(uint32_t id_worker) {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_registrados); i++) {
        t_worker_conectado* worker = list_get(workers_registrados, i);
        if (worker->id_worker == id_worker) {
            encontrado = worker;
            break;
        }
    }
    UNLOCK(&mutex_workers_conectados);
    return encontrado;
}

t_worker_conectado* obtener_worker_por_id_uso_interno(uint32_t id_worker) {
    
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_registrados); i++) {
        t_worker_conectado* worker = list_get(workers_registrados, i);
        if (worker->id_worker == id_worker) {
            encontrado = worker;
            break;
        }
    }
    
    return encontrado;
}

t_worker_conectado* obtener_worker_por_query_id(uint32_t query_id) {
    
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_registrados); i++) {
        t_worker_conectado* worker = list_get(workers_registrados, i);
        if (worker->qid_actual == query_id) {
            encontrado = worker;
            break;
        }
    }
    UNLOCK(&mutex_workers_conectados);
    return encontrado;
}

t_worker_conectado* obtener_worker_libre() {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_registrados); i++) {
        t_worker_conectado* worker = list_get(workers_registrados, i);
        if (worker->qid_actual == QID_NULO && worker->worker_conectado) {
            encontrado = worker;
            break;
        }
    }
    UNLOCK(&mutex_workers_conectados);
    return encontrado;
}

uint32_t get_worker_qid(uint32_t id_worker) {
    
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* worker = obtener_worker_por_id_uso_interno(id_worker);
    UNLOCK(&mutex_workers_conectados);

    return worker->qid_actual;
}


void establecer_worker_desalojado(uint32_t id_worker) {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* worker  = obtener_worker_por_id_uso_interno(id_worker);
    worker->qid_actual = QID_NULO;
    UNLOCK(&mutex_workers_conectados);
}

void marcar_worker_desconectado(uint32_t id_worker) {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* worker  = obtener_worker_por_id_uso_interno(id_worker);
    worker->worker_conectado = false;
    worker->qid_actual = QID_NULO;
    UNLOCK(&mutex_workers_conectados);
}

void marcar_worker_conectado(uint32_t id_worker) {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* worker  = obtener_worker_por_id_uso_interno(id_worker);
    worker->worker_conectado = true;
    UNLOCK(&mutex_workers_conectados);
}

void remover_worker(t_worker_conectado* worker) {
    
    LOCK(&mutex_workers_conectados);
    list_remove_element(workers_registrados, worker);
    UNLOCK(&mutex_workers_conectados);

    free(worker);
}


int get_cant_workers_conectados() {
    return list_size(workers_registrados);
}




