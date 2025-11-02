#include "worker_conexion.h"

#define LIBERACION_WORKER_SIN_EVENTO false
#define LIBERACION_WORKER_CON_EVENTO true


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
    } else if(existente != NULL && !(existente->worker_conectado)) {
        // Marcar como que ya estaba registrado y se volvió a conectar
        log_warning(get_logger(), "[WORKER_CONEXION] ID de WORKER %u se ha vuelto a conectar.", id_worker);
        marcar_worker_conectado(id_worker);
    } else {
        // Registrar si no existía previamente
        registrar_worker(id_worker, cliente_fd);
        log_info(get_logger(), "[WORKER_CONEXION] WORKER %u registrado exitósamente con FD %d", id_worker, cliente_fd);
    }

    t_estado_handshake registrado = HANDSHAKE_OK;
    send(cliente_fd, &registrado, sizeof(t_estado_handshake), 0);

    enviar_evento_planificacion(EVENTO_NUEVO_WORKER_CONECTADO, id_worker, VALOR_NULO_EVENTO, VALOR_NULO_EVENTO);


    while (1) {

        uint32_t query_id;
        uint32_t program_counter;

        int cod_op = recibir_operacion(cliente_fd, get_logger());
        if (cod_op == -1) {
            log_info(get_logger(), "WORKER desconectado, iniciando evento desconexion");
            query_id = get_worker_qid(id_worker);
            enviar_evento_planificacion(EVENTO_WORKER_DESCONECTADO, id_worker, query_id, VALOR_NULO_EVENTO);
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

                switch (aviso->tipo_aviso)
                {
                case NUEVA_LECTURA:
                    char* lectura = aviso->argumento;
                    mandar_lectura_a_query_con_id(lectura, get_worker_qid(id_worker));

                    break;
                    
                case DEVOLUCION_X_INTERRUPCION: 
                    program_counter = atoi(aviso->argumento);
                    query_id = get_worker_qid(id_worker);

                    //TODO: Revisar si esta funcion va aquí o hay que modificar esta lógica
                    //Acordarse del caso desalojo por desconexion query
                    worker_libera_query(id_worker, query_id, program_counter);

                    alta_aviso_confirmacion(INTERRUPCION, query_id, id_worker);

                    break;

                case FINALIZACION_QUERY:
                    program_counter = atoi(aviso->argumento);
                    query_id = get_worker_qid(id_worker);

                    worker_libera_query(id_worker, query_id, program_counter);
                    notificar_finalizacion_a_query_control(query_id);

                    alta_aviso_confirmacion(INTERRUPCION, query_id, id_worker);

                    break;

                case RESPUESTA_SIG_QUERY: 
                    // TODO : Revisar si hay que agregar lógica previamente
                    
                    alta_aviso_confirmacion(PEDIDO_QUERY, -1, id_worker);

                default:
                    break;
                }

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

t_queue* cola_envio_pedidos;

t_dictionary* confirmaciones_por_worker;

pthread_mutex_t mutex_confirmaciones = PTHREAD_MUTEX_INITIALIZER;

void agregar_siguiente_query_a_enviar(t_query* query, t_worker_conectado* worker_libre, t_confirmacion_pedido* conf) {

    t_siguiente_pedido* nuevo_pedido = malloc(sizeof(t_siguiente_pedido));

    t_worker_conectado* worker_a_usar = worker_libre;

    if(worker_a_usar == NULL) {
        log_error(get_logger(), "ERROR FATAL: agregar_siguiente_query_a_enviar() recibió worker NULL");
    }

    nuevo_pedido -> qid = query -> query_id;
    nuevo_pedido -> pc = query -> program_count;
    nuevo_pedido -> query_path = query -> query_path;
    nuevo_pedido -> worker_asignado = worker_a_usar;
    nuevo_pedido -> tipo = PEDIDO_QUERY;
    nuevo_pedido -> confirmacion = conf;
    

    queue_push(cola_envio_pedidos, nuevo_pedido);
    sem_post(sem_envio_pedido_worker_pendiente);

} 

void agregar_pedido_interrupcion(t_worker_conectado* worker, uint32_t query_id, t_confirmacion_pedido* conf) {

    t_siguiente_pedido* nuevo_pedido = malloc(sizeof(t_siguiente_pedido));

    char* mensaje_interrupt = "Pedido Interrupcion; si lo está leyendo hay un error";

    nuevo_pedido -> qid = query_id;
    nuevo_pedido -> pc = -1;
    nuevo_pedido -> query_path = mensaje_interrupt;
    nuevo_pedido -> worker_asignado = worker;
    nuevo_pedido -> tipo = INTERRUPCION;
    nuevo_pedido->confirmacion = conf;

    queue_push(cola_envio_pedidos, nuevo_pedido);
    sem_post(sem_envio_pedido_worker_pendiente);

}

bool enviar_siguiente_pedido(t_worker_conectado* worker, t_pedido_master_worker* sig_pedido) {
    if (!worker || !worker->worker_conectado) {
        log_error(get_logger(), "[CONEXION] No se puede enviar el pedido: worker nula o no conectada.");
        return false;
    }

    t_paquete* paquete = empaquetar_pedido_master_worker(sig_pedido);
    if (!paquete) {
        log_error(get_logger(), "[CONEXION] No se pudo empaquetar el siguiente pedido");
        return false;
    }

    enviar_paquete(paquete, worker->fd_worker);


    log_info(get_logger(), "[CONEXION] Enviado QID %u con PC %u a Worker %u (FD %d)", 
             sig_pedido->query_id, sig_pedido->program_counter, worker->id_worker, worker->fd_worker);

    return true;
}


void* tratar_siguientes_pedidos_a_enviar_worker(void* _) {
    cola_envio_pedidos = queue_create();

    inicializar_sistema_confirmaciones();

    while(true) {
        sem_wait(sem_envio_pedido_worker_pendiente);

        t_siguiente_pedido* sig_pedido = queue_pop(cola_envio_pedidos);
        uint32_t qid_pedido = sig_pedido->qid;
        uint32_t pc_pedido = sig_pedido->pc;
        char* path = sig_pedido->query_path;
        t_worker_conectado* worker = sig_pedido->worker_asignado;
        t_motivo_pedido_master_worker motivo = sig_pedido -> tipo;
        t_confirmacion_pedido* conf = sig_pedido -> confirmacion;

        free(sig_pedido);

        t_pedido_master_worker* pedido = malloc(sizeof(t_pedido_master_worker));
        pedido->query_id= qid_pedido;
        pedido->program_counter = pc_pedido;
        pedido->query_path = path;
        pedido->motivo = motivo;

        char key[32];
        sprintf(key, "%u", worker->id_worker);

        pthread_mutex_lock(&mutex_confirmaciones);
        dictionary_put(confirmaciones_por_worker, key, conf);
        pthread_mutex_unlock(&mutex_confirmaciones);


        if (enviar_siguiente_pedido(worker, pedido)) {
            log_info(get_logger(), "[ENVIO] Pedido enviado a Worker %u. Quien lo llamó esperará confirmación.", 
                     worker->id_worker);
        } else {
            log_error(get_logger(), "[ERROR] Falló el envío a Worker %u", worker->id_worker);
            
            // Si falla el envío, señalizar error
            pthread_mutex_lock(&mutex_confirmaciones);
            dictionary_remove(confirmaciones_por_worker, key);
            pthread_mutex_unlock(&mutex_confirmaciones);
            
            conf->respuesta_recibida = false;
            sem_post(&conf->sem_respuesta);  // Despertar al que espera con error
        }

        free(pedido);
    }

}


void inicializar_sistema_confirmaciones() {
    confirmaciones_por_worker = dictionary_create();
    pthread_mutex_init(&mutex_confirmaciones, NULL);
}

bool asignar_query_a_worker(t_query* query, t_worker_conectado* worker) {
    
    t_confirmacion_pedido* conf = malloc(sizeof(t_confirmacion_pedido));
    sem_init(&conf->sem_respuesta, 0, 0);
    conf->respuesta_recibida = false;
    conf->worker_id = worker->id_worker;
    conf->query_id = query->query_id;
    conf->tipo_pedido = PEDIDO_QUERY;

    log_info(get_logger(), "[DEBUG] Esperando confirmación de Worker %u para QID %u...", 
             worker->id_worker, query->query_id);

    agregar_siguiente_query_a_enviar(query, worker, conf);


    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 10;  // 10 segundos de timeout

    int resultado = sem_timedwait(&conf->sem_respuesta, &timeout);

    if (resultado == 0 && conf->respuesta_recibida) {
        
       return true;

    } else if (resultado == -1 && errno == ETIMEDOUT) {
        log_error(get_logger(), "[ASIGNACION] ⏱️ TIMEOUT esperando Worker %u", worker->id_worker);
        // Manejar timeout

        //Si sale mal retornar false
    } else {
        return false;
    }
    

    char key[32];
    sprintf(key, "%u", worker->id_worker);
    LOCK(&mutex_confirmaciones);
        dictionary_remove(confirmaciones_por_worker, key);
    UNLOCK(&mutex_confirmaciones);
    
    
    sem_destroy(&conf->sem_respuesta);
    free(conf);
}

void alta_aviso_confirmacion(t_motivo_pedido_master_worker motivo_pedido, uint32_t query_id, uint32_t id_worker) {

    char key[32];
    sprintf(key, "%u", id_worker);

    LOCK(&mutex_confirmaciones);
        t_confirmacion_pedido* conf = dictionary_get(confirmaciones_por_worker, key);
        if (conf != NULL && conf->tipo_pedido == motivo_pedido) {
            conf->respuesta_recibida = true;
            sem_post(&conf->sem_respuesta);
            log_info(get_logger(), "[CONFIRMACION] Worker %u realizó una confirmacion para QID %u", 
                    id_worker, query_id);
        }
    UNLOCK(&mutex_confirmaciones);
}