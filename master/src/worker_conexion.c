#include "worker_conexion.h"

uint32_t id_worker;

void* manejar_worker(void* arg) {

    uint32_t id_worker;


    t_conexion_identificada* conexion = (t_conexion_identificada*)arg;
    int cliente_fd = conexion->socket_fd;
    free(conexion);

    
    
    log_info(get_logger(), "Master - WORKER conectado - FD del socket: %d", cliente_fd);
    
    // Enviar confirmación de handshake
    uint32_t confirmacion = HANDSHAKE_OK;
    send(cliente_fd, &confirmacion, sizeof(uint32_t), 0);
    

    if (recv(cliente_fd, &id_worker, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(get_logger(), "[WORKER_CONEXION] No se pudo recibir el ID del WORKER (FD %d)", cliente_fd);
        close(cliente_fd);
        return NULL;
    }

    // Validar si ya existe
    t_worker_conectado* existente = obtener_worker_por_id_uso_externo(id_worker);
    if (existente != NULL && existente->worker_conectado) {
        log_error(get_logger(), "[WORKER_CONEXION] ID de WORKER %u ya está registrado. Desconectando...", id_worker);
        t_estado_handshake ya_registrado = HANDSHAKE_FALLO;
        send(cliente_fd, &ya_registrado, sizeof(t_estado_handshake), 0);
        close(cliente_fd);
        return NULL;
    }

    // Registrar y confirmar OK
    registrar_worker(id_worker, cliente_fd);
    log_info(get_logger(), "[WORKER_CONEXION] WORKER %u registrado exitósamente con FD %d", id_worker, cliente_fd);

    t_estado_handshake registrado = HANDSHAKE_OK;
    send(cliente_fd, &registrado, sizeof(t_estado_handshake), 0);



    while (1) {
        int cod_op = recibir_operacion(cliente_fd, get_logger());
        if (cod_op == -1) {
            log_info(get_logger(), "WORKER desconectado");
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                int size;
                void* buffer = recibir_buffer(&size, cliente_fd);
                if (buffer == NULL) {
                    log_error(get_logger(), "[QUERY] Error al recibir el buffer");
                    return NULL;
                }
                
                t_aviso_worker_master* aviso = desempaquetar_aviso_worker_master(buffer);
                
                if (!aviso) {
                    log_error(get_logger(), "Error al desempaquetar aviso de WORKER");
                    free(buffer);
                    break;
                }

                
                log_info(get_logger(), "Mensaje: %s, Motivo: %d", aviso->argumento, aviso->tipo_aviso);

                break;
                
            default:
                log_warning(get_logger(), "Código de operación desconocido de WORKER: %d", cod_op);
                break;
        }
    }

    close(cliente_fd);
    return NULL;
}


/*  --------------------------------------------------------------------------------------
    ---------------------------- ENVIOS Y PEDIDOS ---------------------------------------- 
    -------------------------------------------------------------------------------------- */

t_queue* cola_envio_queries;

void agregar_siguiente_query_a_enviar(t_query* query, t_worker_conectado* worker_libre) {

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
        log_error(get_logger(), "[CONEXION] No se puede enviar el query: worker nula o no conectada.");
        return false;
    }

    t_paquete* paquete = empaquetar_pedido_master_worker(sig_pedido);
    if (!paquete) {
        log_error(get_logger(), "[CONEXION] No se pudo empaquetar el siguiente query");
        return false;
    }

    enviar_paquete(paquete, worker->fd_worker);

    worker->qid_actual = sig_pedido->query_id;

    log_info(get_logger(), "[CONEXION] Enviado QID %u con PC %u a Worker %u (FD %d)", 
             sig_pedido->query_id, sig_pedido->program_counter, worker->id_worker, worker->fd_worker);

    return true;
}


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

        //TODO no hardcodear
        pedido->motivo = PEDIDO_QUERY;


        if (enviar_siguiente_query(worker, pedido)) {
            log_info(get_logger(), "[DEBUG] Query (ID: %u) enviado a Worker (ID: %u)", pedido->query_id, worker->id_worker);
        } else {
            log_error(get_logger(), "[ERROR] Falló el envío del query (ID: %u) a Worker (ID: %u)", 
                                    pedido->query_id, worker->id_worker);
        }

        free(pedido);
    }

}