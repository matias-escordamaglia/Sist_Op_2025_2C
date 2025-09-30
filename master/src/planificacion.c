#include "planificacion.h"

t_list* cola_ready = NULL;
t_list* cola_exec = NULL;
t_list* cola_exit = NULL;

pthread_mutex_t mutex_cola_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exit = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_estado_critico = PTHREAD_MUTEX_INITIALIZER;

void inicializar_listas_planificacion() {
    if (cola_ready == NULL)
    {
        cola_ready = list_create();
    }

    if (cola_exec == NULL)
    {
        cola_exec = list_create();
    }
    
    if (cola_exit == NULL)
    {
        cola_exit = list_create();
    }
}

uint64_t timestamp_actual_en_milisegundos() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void crear_nuevo_query(char* query_path, uint32_t prioridad, int conexion) {
    t_query* nuevo_query = crear_query(query_path, prioridad, conexion);

    // TODO REPETIDO 1 hilo con temporizador para aging posiblemente
    //inicializar_temporizador_query(nuevo_query);
    t_elemento_cola* nuevo_elemento = crear_nuevo_elemento(nuevo_query);

    LOCK(&mutex_cola_ready);
        list_add(cola_ready, nuevo_elemento);
        log_info(get_logger(), "Se crea nuevo Query con ID (%d) - Estado: READY", nuevo_query->query_id);
    UNLOCK(&mutex_cola_ready);

    //TODO REVISAR ESTOS 0s
    enviar_evento_planificacion(EVENTO_NUEVA_QUERY, -1, nuevo_query->query_id, 0);
}

t_elemento_cola* crear_nuevo_elemento(t_query* query) {
    t_elemento_cola* nuevo_elemento = malloc(sizeof(t_elemento_cola));

    nuevo_elemento->query = query;
    //TODO REPETIDO 1: Aquí seguro vaya un temporizador u algún hilo para lo de aging si es que está activo
    nuevo_elemento->tiempo_llegada = timestamp_actual_en_milisegundos();

    return nuevo_elemento;
    
}

t_elemento_cola* buscar_y_remover_por_qid(t_list* lista, uint32_t qid) {
    if (lista == NULL || list_is_empty(lista)) {
        return NULL;
    }
    
    for (int i = 0; i < list_size(lista); i++) {
        t_elemento_cola* elemento = list_get(lista, i);
        if (elemento != NULL && elemento->query != NULL && elemento->query->query_id == qid) {
            list_remove_element(lista, elemento);
            return elemento;
        }
    }
    return NULL;
}

t_elemento_cola* obtener_mas_antiguo(t_list* cola) {
    if (list_is_empty(cola)) {
        return NULL;
    }
    
    t_elemento_cola* mas_antiguo = list_get(cola, 0);

    for (int i = 1; i < list_size(cola); i++) {
        t_elemento_cola* actual = list_get(cola, i);
        if (actual->tiempo_llegada < mas_antiguo->tiempo_llegada) {
            mas_antiguo = actual;
        }
    }

    return mas_antiguo;
}


void *main_planificacion() {
    //Iniciar semaforos corto plazo -> iniciar_sem_cp();

    inicializar_listas_planificacion();
    inicializar_cola_eventos();
    
    pthread_t hilo_eventos;
    pthread_create(&hilo_eventos, NULL, manejar_eventos_planificacion, NULL);
    pthread_detach(hilo_eventos);

    char* algoritmo_planificacion = config_get_string_value(get_config(), "ALGORITMO_PLANIFICACION");

    if (strcmp(algoritmo_planificacion, "FIFO") == 0)
    {
        planificar_por_fifo();
    }
    else if (strcmp(algoritmo_planificacion, "PRIORIDADES") == 0)
    {
        planificar_por_prioridades();
    } else 
    {
        log_error(get_logger(), "ERROR al querer establecer un algoritmo de planificación.");
    }
    return (void *)1;
}


void planificar_por_fifo() {
    while(true) {
        // Esperar que haya trabajo que hacer
        sem_wait(sem_trabajo_planificacion);
        
        // Intentar hacer todas las asignaciones posibles
        intentar_asignaciones_fifo();
    }
}

void intentar_asignaciones_fifo() {
    
    LOCK(&mutex_estado_critico);

    while (true) {
        
        bool hay_queries = false;
        
        LOCK(&mutex_cola_ready);
        hay_queries = !list_is_empty(cola_ready);
        UNLOCK(&mutex_cola_ready);
        
        if (!hay_queries) {
            break;
        }
        
        // Verificar workers disponibles (sin bloquear)
        if (sem_trywait(cant_workers_libres) != 0) {
            break; 
        }
        
        // Hay query Y worker disponible, proceder con asignación
        t_elemento_cola* mas_antiguo = NULL;
        
        LOCK(&mutex_cola_ready);
        mas_antiguo = obtener_mas_antiguo(cola_ready);
        if (mas_antiguo != NULL) {
            list_remove_element(cola_ready, mas_antiguo);
        }
        UNLOCK(&mutex_cola_ready);
        
        if (mas_antiguo == NULL) {
            sem_post(cant_workers_libres);
            break;
        }
        
        // Obtener worker y verificar que siga conectado
        t_worker_conectado* worker_libre = obtener_worker_libre();
        if (worker_libre == NULL || !(worker_libre->worker_conectado)) {
            // Worker se desconectó entre tanto, devolver query a ready
            LOCK(&mutex_cola_ready);
            list_add_in_index(cola_ready, 0, mas_antiguo); // Al frente para FIFO
            UNLOCK(&mutex_cola_ready);
            continue;
        }
        
        agregar_siguiente_query_a_enviar(mas_antiguo->query, worker_libre);
        
        LOCK(&mutex_cola_exec);
        list_add(cola_exec, mas_antiguo);
        UNLOCK(&mutex_cola_exec);
        
        log_info(get_logger(), "Query %d asignado a Worker %d", 
                mas_antiguo->query->query_id, worker_libre->id_worker);
    }

    UNLOCK(&mutex_estado_critico);
}


void planificar_por_prioridades() {
    while(true) {
        sem_wait(sem_trabajo_planificacion);
        intentar_asignaciones_prioridades();
    }
}

void intentar_asignaciones_prioridades() {
    // TODO Similar a FIFO pero ordenando por prioridad
}



/*-------------------------------EVENTOS-----------------------------------*/


t_queue* cola_eventos_planificacion;
pthread_mutex_t mutex_cola_eventos;

void* manejar_eventos_planificacion(void* args) {
    while (true) {
        sem_wait(sem_eventos_pendientes);
        
        LOCK(&mutex_cola_eventos);
        if (queue_is_empty(cola_eventos_planificacion)) {
            UNLOCK(&mutex_cola_eventos);
            continue;
        }
        
        t_evento_planificacion* evento = queue_pop(cola_eventos_planificacion);
        UNLOCK(&mutex_cola_eventos);
        
        bool despertar_planificador = false;
        
        switch (evento->tipo) {
            case EVENTO_WORKER_DESCONECTADO:
                manejar_worker_desconectado(evento->worker_id, evento->query_id);
                // No despertar planificador, perdimos un worker
                break;
                
            case EVENTO_QUERY_CONTROL_DESCONECTADO:
                manejar_query_control_desconectado(evento->query_control_id, evento->query_id);
                // No despertar planificador, perdimos queries
                break;
                
            case EVENTO_NUEVA_QUERY:
                // Nueva query en ready, despertar planificador
                despertar_planificador = true;
                break;
                
            case EVENTO_WORKER_LIBERADO:
                // Worker terminó su trabajo, despertar planificador
                despertar_planificador = true;
                sem_post(cant_workers_libres);
                break;
                
            case EVENTO_NUEVO_WORKER_CONECTADO:
                // Nuevo worker disponible, despertar planificador
                despertar_planificador = true;
                sem_post(cant_workers_libres);
                break;
        }
        
        if (despertar_planificador) {
            sem_post(sem_trabajo_planificacion);
        }
        
        free(evento);
    }
    return NULL;
}

void inicializar_cola_eventos() {
    if (cola_eventos_planificacion == NULL) {
        cola_eventos_planificacion = queue_create();
    }
}

void enviar_evento_planificacion(t_tipo_evento tipo, uint32_t worker_id, uint32_t query_id, uint32_t qc_id) {
    t_evento_planificacion* evento = malloc(sizeof(t_evento_planificacion));
    evento->tipo = tipo;
    evento->worker_id = worker_id;
    evento->query_id = query_id;
    evento->query_control_id = qc_id;

    LOCK(&mutex_cola_eventos);
    queue_push(cola_eventos_planificacion, evento);
    UNLOCK(&mutex_cola_eventos);

    sem_post(sem_eventos_pendientes);
}

void manejar_worker_desconectado(uint32_t worker_id, uint32_t query_id_ejecutando) {

    LOCK(&mutex_estado_critico);

    log_info(get_logger(), "Worker %d desconectado", worker_id);

    if (query_id_ejecutando >= 0) {
        t_elemento_cola* elemento = NULL;
        

        LOCK(&mutex_cola_exec);
        elemento = buscar_y_remover_por_qid(cola_exec, query_id_ejecutando);
        UNLOCK(&mutex_cola_exec);
        
        if (elemento != NULL) {
            
            // Mover a EXIT
            LOCK(&mutex_cola_exit);
            list_add(cola_exit, elemento);
            UNLOCK(&mutex_cola_exit);
            
            log_info(get_logger(), "Query %d movido a EXIT por desconexión de worker", 
                    query_id_ejecutando);
            
            
            notificar_error_a_query_control(elemento->query->conexion);
        }
    }
    
    // Marcar worker como desconectado también hace wait al semaforo de cantidad
    marcar_worker_desconectado(worker_id);

    UNLOCK(&mutex_estado_critico);
    
}

void manejar_query_control_desconectado(uint32_t qc_id, uint32_t query_id_activo) {

    LOCK(&mutex_estado_critico);

    log_info(get_logger(), "Query Control %d desconectado", qc_id);
    
    if (query_id_activo != 0) {
        t_elemento_cola* elemento = NULL;
        
        // Buscar en READY primero
        LOCK(&mutex_cola_ready);
        elemento = buscar_y_remover_por_qid(cola_ready, query_id_activo);
        UNLOCK(&mutex_cola_ready);
        
        if (elemento != NULL) {
            // Estaba en ready, simplemente cancelar
            
            LOCK(&mutex_cola_exit);
            list_add(cola_exit, elemento);
            UNLOCK(&mutex_cola_exit);
            
        } else {
            // Buscar en EXEC
            LOCK(&mutex_cola_exec);
            elemento = buscar_y_remover_por_qid(cola_exec, query_id_activo);
            UNLOCK(&mutex_cola_exec);
            
            if (elemento != NULL) {
                // Estaba ejecutándose, desalojar worker
                
                t_worker_conectado* worker_a_desalojar = obtener_worker_por_query_id(query_id_activo);
                uint32_t worker_id = worker_a_desalojar->id_worker; 
                
                //TODO enviar_desalojo_a_worker(worker_id);
            
                
                LOCK(&mutex_cola_exit);
                list_add(cola_exit, elemento);
                UNLOCK(&mutex_cola_exit);
                
                // Liberar worker
                sem_post(cant_workers_libres);
                
                log_info(get_logger(), "Query %d cancelado y worker %d desalojado", 
                        query_id_activo, worker_id);
            }
        }
    }

    UNLOCK(&mutex_estado_critico);
}

void worker_libera_query(uint32_t worker_id, uint32_t query_id, uint32_t pc) {
    LOCK(&mutex_estado_critico);
    
    t_elemento_cola* elemento = NULL;
    
    LOCK(&mutex_cola_exec);
    elemento = buscar_y_remover_por_qid(cola_exec, query_id);
    UNLOCK(&mutex_cola_exec);
    
    if (elemento != NULL) {
        
        LOCK(&mutex_cola_exit);
        list_add(cola_exit, elemento);
        UNLOCK(&mutex_cola_exit);
        
        log_info(get_logger(), "Query %d completado por worker %d", query_id, worker_id);
    }
    
    UNLOCK(&mutex_estado_critico);
    
    enviar_evento_planificacion(EVENTO_WORKER_LIBERADO, worker_id, query_id, pc);
}