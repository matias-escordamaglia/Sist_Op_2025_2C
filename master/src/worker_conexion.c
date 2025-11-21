#include "worker_conexion.h"

#define LIBERACION_WORKER_SIN_EVENTO false
#define LIBERACION_WORKER_CON_EVENTO true

pthread_mutex_t mutex_confirmaciones = PTHREAD_MUTEX_INITIALIZER;

t_dictionary* confirmaciones_por_worker;

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

            char key[32];
            sprintf(key, "%u", id_worker);
            LOCK(&mutex_confirmaciones);
            t_confirmacion_pedido* conf = dictionary_get(confirmaciones_por_worker, key);
            if (conf != NULL && conf->tipo_pedido == INTERRUPCION) {
                conf->dato_respuesta = 0;  
                conf->respuesta_recibida = false;
                sem_post(&conf->sem_respuesta);
                log_warning(get_logger(), "Worker se desconectó durante desalojo pendiente");
            }
            UNLOCK(&mutex_confirmaciones);

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
                    uint32_t query_id_actual = get_worker_qid(id_worker);

                    int intentos = 0;
                    while (query_id_actual == -1 && intentos < 10) {
                        log_warning(get_logger(), "[RACE CONDITION] Worker %d envió lectura pero figura sin QID. Reintento (%d/10)...", id_worker, intentos+1);
                        usleep(100000);
                        
                        query_id_actual = get_worker_qid(id_worker);
                        intentos++;
                    }

                    if (query_id_actual == -1) {
                        log_error(get_logger(), "ERROR: Se recibió lectura del Worker %d pero sigue sin QID asignado tras reintentos.", id_worker);
                    } else {
                        mandar_lectura_a_query_con_id(lectura, query_id_actual);
                    }

                    break;
                    
                case DEVOLUCION_X_INTERRUPCION: 
                    program_counter = atoi(aviso->argumento);
                    query_id = get_worker_qid(id_worker);

                    /*
                    La lógica de reubicar los querys de las colas está en la planificación ya, tanto para el caso
                    de query_desconectado como el de desalojo por algoritmo
                    */
                    establecer_worker_desalojado(id_worker);

                    alta_aviso_confirmacion(INTERRUPCION, query_id, id_worker, program_counter);

                    break;

                case FINALIZACION_QUERY:
                    program_counter = atoi(aviso->argumento);
                    query_id = get_worker_qid(id_worker);

                    worker_libera_query_finalizado(id_worker, query_id, program_counter);
                    notificar_finalizacion_a_query_control(query_id);


                    /*Está la confirmación para el caso en el que se quiera pedir una interrupción pero la
                    misma justo finalizaba*/
                    alta_aviso_confirmacion(INTERRUPCION, query_id, id_worker, -1);

                    break;

                case RESPUESTA_SIG_QUERY: 
                    
                    uint32_t resultado = atoi(aviso->argumento);
                    alta_aviso_confirmacion(PEDIDO_QUERY, -1, id_worker, resultado);

                    break;

                
                case DESALOJO_QUERY_DIFERENTE_RESPUESTA: 
                        
                    uint32_t query_id_real = atoi(aviso->argumento);
                        
                    char key[32];
                    sprintf(key, "%u", id_worker);
                    
                    LOCK(&mutex_confirmaciones);
                    t_confirmacion_pedido* conf = dictionary_get(confirmaciones_por_worker, key);
                    if (conf != NULL && conf->tipo_pedido == INTERRUPCION) {
                        conf->dato_respuesta = query_id_real;
                        conf->respuesta_recibida = true;
                        sem_post(&conf->sem_respuesta);
                        
                        log_warning(get_logger(), 
                                    "[DESALOJO] Worker %u tiene Query %d (esperábamos %d)", 
                                    id_worker, query_id_real, conf->query_id);
                    }
                    UNLOCK(&mutex_confirmaciones);
                    break;
                    
                case ERROR_QUERY:

                    query_id = get_worker_qid(id_worker);

                    char* mensaje_error = aviso->argumento;

                    worker_libera_query_finalizado(id_worker, query_id, -1);
                    notificar_finalizacion_especial_a_query_control(query_id, mensaje_error);

                    break;

                default:
                    break;
                }

                liberar_aviso_completo(aviso);

                break;
                
            default:
                log_warning(get_logger(), "Código de operación desconocido de WORKER: %d", cod_op);
                break;
        }
    }

    close(cliente_fd);
    return NULL;
}

void liberar_aviso_completo(t_aviso_worker_master* aviso) {
    free(aviso->argumento);
    free(aviso);
}



/*  --------------------------------------------------------------------------------------
    ---------------------------- ENVIOS Y PEDIDOS ---------------------------------------- 
    -------------------------------------------------------------------------------------- */

t_queue* cola_envio_pedidos;

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
            log_info(get_logger(), "[ENVIO] Pedido enviado a Worker %u", 
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

    bool exito_asignacion = false;
    
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
    
    int reintentos = 0;
    const int MAX_REINTENTOS = 3;

    do {

        int resultado = sem_timedwait(&conf->sem_respuesta, &timeout);

        if (resultado == 0 && conf->respuesta_recibida) {
            
            switch (conf->dato_respuesta) {
                case OK:
                    exito_asignacion = true;
                    break;

                case ERROR:
                    exito_asignacion = false;
                    break;
                
                default:
                    log_error(get_logger(), "ERROR EN CONFIRMACION: se recibió una respuesta inesperada desde worker");
                    break;
            }

            break;
            

        } else if (resultado == -1 && errno == ETIMEDOUT) {
            log_error(get_logger(), "[ASIGNACION] TIMEOUT esperando Worker %u; reintentando...", worker->id_worker);
            
            reintentos++;

            if (reintentos < MAX_REINTENTOS) {
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 10;
            }
            
        } else {
            break;
        }

    } while (reintentos < MAX_REINTENTOS);
    

    char key[32];
    sprintf(key, "%u", worker->id_worker);
    LOCK(&mutex_confirmaciones);
        dictionary_remove(confirmaciones_por_worker, key);
    UNLOCK(&mutex_confirmaciones);
    
    
    sem_destroy(&conf->sem_respuesta);
    free(conf);

    return exito_asignacion;
}

void alta_aviso_confirmacion(t_motivo_pedido_master_worker motivo_pedido, uint32_t query_id, 
                                uint32_t id_worker, uint32_t dato_extra) {

    char key[32];
    sprintf(key, "%u", id_worker);

    LOCK(&mutex_confirmaciones);
        t_confirmacion_pedido* conf = dictionary_get(confirmaciones_por_worker, key);
        if (conf != NULL && conf->tipo_pedido == motivo_pedido) {

            if (motivo_pedido == PEDIDO_QUERY && dato_extra == OK) { 
                t_worker_conectado* worker = obtener_worker_por_id_uso_externo(id_worker);
                if (worker) {
                    
                    asociar_qid_a_worker(conf->query_id, worker); 
                    
                    log_info(get_logger(), "[DEBUG] Race Condition evitada: QID %d asociado a Worker %d al recibir confirmación", 
                            conf->query_id, id_worker);
                } else {
                    log_error(get_logger(), "ERROR; no se encontró dato worker pese a que se llama desde un worker");
                }
            }

            conf->respuesta_recibida = true;
            conf->dato_respuesta = dato_extra;
            sem_post(&conf->sem_respuesta);
            log_info(get_logger(), "[DEBUG] Worker %u realizó una confirmacion para QID %u", 
                    id_worker, query_id);
        }
    UNLOCK(&mutex_confirmaciones);
}


/*  --------------------------------------------------------------------------------------
    ----------------------------------- DESALOJOS ---------------------------------------- 
    -------------------------------------------------------------------------------------- */


t_respuesta_desalojo solicitar_desalojo_bloqueante(t_worker_conectado* worker, uint32_t query_id_esperado) {
    t_respuesta_desalojo respuesta_error = {
        .resultado = DESALOJO_ERROR_FATAL,
        .pc = 0,
        .query_id_actual = 0
    };
    
    if (!worker->worker_conectado) {
        respuesta_error.resultado = DESALOJO_WORKER_DESCONECTADO;
        return respuesta_error;
    }
    
    // Crear confirmación
    t_confirmacion_pedido* conf = malloc(sizeof(t_confirmacion_pedido));
    sem_init(&conf->sem_respuesta, 0, 0);
    conf->respuesta_recibida = false;
    conf->worker_id = worker->id_worker;
    conf->query_id = query_id_esperado;
    conf->tipo_pedido = INTERRUPCION;
    conf->dato_respuesta = 0;
    
    agregar_pedido_interrupcion(worker, query_id_esperado, conf);
    
    log_info(get_logger(), "[DESALOJO] Esperando confirmación de Worker %u...", worker->id_worker);
    
    // Esperar con timeout y reintentos
    struct timespec timeout;
    int resultado;
    int reintentos = 0;
    const int MAX_REINTENTOS = 3;
    bool exito = false;

    t_respuesta_desalojo respuesta_final;

    do {
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 5;
        
        resultado = sem_timedwait(&conf->sem_respuesta, &timeout);
        
        if (resultado == 0) {
            
            if (conf->respuesta_recibida && conf->dato_respuesta > 0) {

                if (dato_es_query_diferente(conf->dato_respuesta, query_id_esperado)) {
                    respuesta_final.resultado = DESALOJO_QUERY_DIFERENTE;
                    respuesta_final.query_id_actual = conf->dato_respuesta;
                    respuesta_final.pc = 0;
                    log_warning(get_logger(), "[DESALOJO] Worker %u tiene Query %d diferente", 
                               worker->id_worker, conf->dato_respuesta);
                } else {
                    respuesta_final.resultado = DESALOJO_EXITOSO;
                    respuesta_final.pc = conf->dato_respuesta;  // Es el PC
                    respuesta_final.query_id_actual = query_id_esperado;
                    log_info(get_logger(), "[DESALOJO] Worker %u confirmó con PC=%d", 
                            worker->id_worker, respuesta_final.pc);
                }
                exito = true;
                break;
                
            } else if (!conf->respuesta_recibida) {
                
                if (!worker->worker_conectado) {
                    respuesta_final.resultado = DESALOJO_WORKER_DESCONECTADO;
                    log_warning(get_logger(), "[DESALOJO] Worker %u se desconectó", worker->id_worker);
                    exito = true;
                } else {
                    log_error(get_logger(), "[DESALOJO] se llegó a punto muerto en el código, revisar camino del caso de uso");
                    while(true) {
                        printf("ERROR FATAL EN DESALOJO");
                        sleep(5);
                    }
                }
                
                break;
            }
            
        } else if (resultado == -1 && errno == ETIMEDOUT) {
            reintentos++;
            log_warning(get_logger(), "[DESALOJO] TIMEOUT Worker %u (reintento %d/%d)", 
                    worker->id_worker, reintentos, MAX_REINTENTOS);
            
            // Verificación crítica
            if (!worker->worker_conectado) {
                log_error(get_logger(), "Worker %d se desconectó (detectado en timeout %d)", 
                         worker->id_worker, reintentos);
                respuesta_final.resultado = DESALOJO_WORKER_DESCONECTADO;
                exito = true;
                break;
            }
        } else {
            break;
        }
        
    } while (reintentos < MAX_REINTENTOS);

    
    if (!exito) {
        if (resultado == -1 && errno == ETIMEDOUT) {
            respuesta_final.resultado = DESALOJO_TIMEOUT;
            log_error(get_logger(), "[DESALOJO] TIMEOUT FINAL Worker %u", worker->id_worker);
            
            if (worker->worker_conectado) {
                log_error(get_logger(), "INCONSISTENCIA CRÍTICA: Worker %d no responde", worker->id_worker);
                marcar_worker_desconectado(worker->id_worker);
                sem_wait(cant_workers_libres);
            }
        } else {
            respuesta_final.resultado = DESALOJO_ERROR_FATAL;
        }
    }
    
    // Limpiar
    char key[32];
    sprintf(key, "%u", worker->id_worker);
    pthread_mutex_lock(&mutex_confirmaciones);
    dictionary_remove(confirmaciones_por_worker, key);
    pthread_mutex_unlock(&mutex_confirmaciones);
    
    sem_destroy(&conf->sem_respuesta);
    free(conf);
    
    return respuesta_final;
}

bool dato_es_query_diferente(uint32_t dato, uint32_t query_id_esperado) {

    if (dato == query_id_esperado) {
        return false;
    }
    
    return false;
}