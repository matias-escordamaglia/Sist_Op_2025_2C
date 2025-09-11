#include "worker_manager.h"

#define QID_NULO -1

t_queue* cola_envio_queries;

t_list* workers_conectados;

pthread_mutex_t mutex_workers_conectados = PTHREAD_MUTEX_INITIALIZER;

void iniciar_worker_manager() {
    workers_conectados = list_create();
}

void registrar_worker(uint32_t id_worker, int cliente_fd) {
    t_worker_conectado* worker;

    worker = malloc(sizeof(t_worker_conectado));
    worker -> qid_actual = QID_NULO;
    worker -> fd_worker = cliente_fd;
    worker -> id_worker = id_worker;
    worker -> worker_conectado = true;

    LOCK(&mutex_workers_conectados);

    list_add(workers_conectados, worker);

    sem_post(cant_workers_libres);

    UNLOCK(&mutex_workers_conectados);
}

t_worker_conectado* obtener_worker_por_id_uso_externo(uint32_t id_worker) {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_conectados); i++) {
        t_worker_conectado* worker = list_get(workers_conectados, i);
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
    for (int i = 0; i < list_size(workers_conectados); i++) {
        t_worker_conectado* worker = list_get(workers_conectados, i);
        if (worker->id_worker == id_worker) {
            encontrado = worker;
            break;
        }
    }
    
    return encontrado;
}

t_worker_conectado* obtener_worker_libre() {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* encontrado = NULL;
    for (int i = 0; i < list_size(workers_conectados); i++) {
        t_worker_conectado* worker = list_get(workers_conectados, i);
        if (worker->qid_actual == QID_NULO) {
            encontrado = worker;
            break;
        }
    }
    UNLOCK(&mutex_workers_conectados);
    return encontrado;
}


void establecer_worker_desalojado(uint32_t id_worker) {
    LOCK(&mutex_workers_conectados);
    t_worker_conectado* worker  = obtener_worker_por_id_uso_interno(id_worker);
    worker->qid_actual = QID_NULO;
    UNLOCK(&mutex_workers_conectados);
}

void remover_worker(t_worker_conectado* worker) {
    
    LOCK(&mutex_workers_conectados);
    list_remove_element(workers_conectados, worker);
    UNLOCK(&mutex_workers_conectados);

    free(worker);
}



/* --------------------- ENVIOS Y PEDIDOS --------------------------------- */

void agregar_siguiente_proceso_a_enviar(t_query* query, t_worker_conectado* worker_libre) {

    t_siguiente_pedido* nuevo_pedido = malloc(sizeof(t_siguiente_pedido));

    t_worker_conectado* worker_a_usar = worker_libre;

    if(worker_a_usar == NULL) {
        worker_a_usar = obtener_worker_libre();
    }

    nuevo_pedido -> qid = query -> query_id;
    nuevo_pedido -> pc = query -> program_count;
    nuevo_pedido -> query_path = query -> query_path;
    nuevo_pedido -> worker_asignado = worker_a_usar;
    

    queue_push(cola_envio_queries, nuevo_pedido);
    sem_post(sem_envio_query_pendiente);

} 

bool enviar_siguiente_query(t_worker_conectado* worker, t_pedido_master_worker* sig_pedido) {
    if (!worker || !worker->worker_conectado) {
        log_error(get_logger(), "[MANAGER] No se puede enviar el query: worker nula o no conectada.");
        return false;
    }

    t_paquete* paquete = empaquetar_pedido_master_worker(sig_pedido);
    if (!paquete) {
        log_error(get_logger(), "[MANAGER] No se pudo empaquetar el siguiente query");
        return false;
    }

    enviar_paquete(paquete, worker->fd_worker);

    worker->qid_actual = sig_pedido->query_id;

    log_info(get_logger(), "[MANAGER] Enviado QID %u con PC %u a Worker %u (FD %d)", 
             sig_pedido->query_id, sig_pedido->program_counter, worker->id_worker, worker->fd_worker);

    return true;
}


//TODO levantar esto en un hilo en master.c
void* tratar_siguientes_queries_a_enviar(void* _) {
    cola_envio_queries = queue_create();

    while(true) {
        sem_wait(sem_envio_query_pendiente);

        t_siguiente_pedido* sig_pedido = queue_pop(cola_envio_queries);
        uint32_t qid_pedido = sig_pedido->qid;
        uint32_t pc_pedido = sig_pedido->pc;
        char* path = sig_pedido->query_path;
        t_worker_conectado* worker = sig_pedido->worker_asignado;
        free(sig_pedido);

        t_pedido_master_worker* pedido = malloc(sizeof(t_pedido_master_worker));
        pedido->query_id= qid_pedido;
        pedido->program_counter = pc_pedido;
        pedido->query_path = path;


        if (enviar_siguiente_query(worker, pedido)) {
            log_info(get_logger(), "[DEBUG] Query (ID: %u) enviado a Worker (ID: %u)", pedido->query_id, worker->id_worker);
        } else {
            log_error(get_logger(), "[ERROR] Falló el envío del query (ID: %u) a Worker (ID: %u)", 
                                    pedido->query_id, worker->id_worker);
        }

        free(pedido);
    }

}