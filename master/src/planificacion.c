#include "planificacion.h"

t_list* cola_ready = NULL;
t_list* cola_exec = NULL;
t_list* cola_exit = NULL;

pthread_mutex_t mutex_cola_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exit = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_estado_critico = PTHREAD_MUTEX_INITIALIZER;

int tiempo_aging_ms;

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

t_query* crear_nuevo_query(char* query_path, uint32_t prioridad, int conexion) {
    t_query* nuevo_query = crear_query(query_path, prioridad, conexion);

    t_elemento_cola* nuevo_elemento = crear_nuevo_elemento(nuevo_query);

    LOCK(&mutex_cola_ready);
        list_add(cola_ready, nuevo_elemento);
        log_info(get_logger(), "Se crea nuevo Query con ID (%d) - Estado: READY", nuevo_query->query_id);
    UNLOCK(&mutex_cola_ready);

    enviar_evento_planificacion(EVENTO_NUEVA_QUERY, VALOR_NULO_EVENTO, nuevo_query->query_id, 0);

    return nuevo_query;
}

t_elemento_cola* crear_nuevo_elemento(t_query* query) {
    t_elemento_cola* nuevo_elemento = malloc(sizeof(t_elemento_cola));

    nuevo_elemento->query = query;
    nuevo_elemento->tiempo_llegada = timestamp_actual_en_milisegundos();
    nuevo_elemento->prioridad_efectiva = query->prioridad;
    nuevo_elemento->ultimo_aging = nuevo_elemento->tiempo_llegada;

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

    while (true) {
        
        if (sem_trywait(cant_workers_libres) != 0) {
            break;
        }

        t_elemento_cola* mas_antiguo = NULL;
        
        LOCK(&mutex_cola_ready);
        if (list_is_empty(cola_ready)) {
            UNLOCK(&mutex_cola_ready);
            sem_post(cant_workers_libres);
            break;
        }
        mas_antiguo = obtener_mas_antiguo(cola_ready);
        list_remove_element(cola_ready, mas_antiguo);
        UNLOCK(&mutex_cola_ready);
        
        t_worker_conectado* worker_libre = obtener_worker_libre();

        LOCK(&mutex_estado_critico);

        if (worker_libre == NULL || !(worker_libre->worker_conectado)) {

            LOCK(&mutex_cola_ready);
            list_add_in_index(cola_ready, 0, mas_antiguo); // Al frente para FIFO
            UNLOCK(&mutex_cola_ready);

            UNLOCK(&mutex_estado_critico);

            sem_post(cant_workers_libres);
            continue;
        }
        
        if (asignar_query_a_worker(mas_antiguo->query, worker_libre)) {
                asociar_qid_a_worker(mas_antiguo->query->query_id, worker_libre);

                LOCK(&mutex_cola_exec);
                    list_add(cola_exec, mas_antiguo);
                UNLOCK(&mutex_cola_exec);
            }

        UNLOCK(&mutex_estado_critico);
        
        log_info(get_logger(), "Query %d asignado a Worker %d", 
                mas_antiguo->query->query_id, worker_libre->id_worker);
    }
}


void planificar_por_prioridades() {

    pthread_t hilo_aging;

    pthread_create(&hilo_aging, NULL, main_aging, NULL);
    pthread_detach(hilo_aging);

    while(true) {
        sem_wait(sem_trabajo_planificacion);
        intentar_asignaciones_prioridades();
    }
}

void intentar_asignaciones_prioridades() {
    LOCK(&mutex_estado_critico);
    
    while (true) {
        
        verificar_y_aplicar_aging_si_corresponde();
        
        // 1. Obtener query MÁS ANTIGUO de mayor prioridad en READY
        t_elemento_cola* query_candidata = obtener_query_mas_prioritaria_mas_antigua();
        
        if (query_candidata == NULL) {
            break; // No hay queries en ready
        }

        
        // 2. Verificar si hay worker libre
        if (sem_trywait(cant_workers_libres) == 0) {

            t_worker_conectado* worker_libre = obtener_worker_libre();
            
            if (worker_libre == NULL || !worker_libre->worker_conectado) {
                sem_post(cant_workers_libres);
                break;
            }
            

            LOCK(&mutex_cola_ready);
            list_remove_element(cola_ready, query_candidata);
            UNLOCK(&mutex_cola_ready);
            
            
            if (asignar_query_a_worker(query_candidata->query, worker_libre)) {
                asociar_qid_a_worker(query_candidata->query->query_id, worker_libre);

                LOCK(&mutex_cola_exec);
                    list_add(cola_exec, query_candidata);
                UNLOCK(&mutex_cola_exec);
            }

            log_info(get_logger(), 
                    "Query %d (prioridad=%d, PC=%d) asignado a Worker %d", 
                    query_candidata->query->query_id,
                    query_candidata->prioridad_efectiva,
                    query_candidata->query->program_count,
                    worker_libre->id_worker);
            
            continue; // Se analiza nuevamente por si hay más asignaciones posibles
        }
        

        // 3. NO HAY WORKERS LIBRES - buscar víctima para desalojar
        t_elemento_cola* query_victima = obtener_victima_desalojo(query_candidata->prioridad_efectiva);
        
        if (query_victima == NULL) {
            break;
        }
        
        // 4. DESALOJAR
        log_info(get_logger(), 
                "Desalojo: Query %d (p=%d, antigüedad=%ld) desalojará a Query %d (p=%d, antigüedad=%ld)",
                query_candidata->query->query_id,
                query_candidata->prioridad_efectiva,
                query_candidata->tiempo_llegada,
                query_victima->query->query_id,
                query_victima->prioridad_efectiva,
                query_victima->tiempo_llegada);
        

        LOCK(&mutex_cola_ready);
        list_remove_element(cola_ready, query_candidata);
        UNLOCK(&mutex_cola_ready);
        

        LOCK(&mutex_cola_exec);
        list_remove_element(cola_exec, query_victima);
        UNLOCK(&mutex_cola_exec);
        

        t_worker_conectado* worker_a_desalojar = 
            obtener_worker_por_query_id(query_victima->query->query_id);
        

        uint32_t pc_recibido = solicitar_desalojo_bloqueante(worker_a_desalojar, query_victima->query->query_id);
        
        query_victima->query->program_count = pc_recibido;
        
        LOCK(&mutex_cola_ready);
        agregar_query_ordenada(cola_ready, query_victima);
        UNLOCK(&mutex_cola_ready);
        
        if (asignar_query_a_worker(query_candidata->query, worker_a_desalojar)) {
            asociar_qid_a_worker(query_candidata->query->query_id, worker_a_desalojar);

            LOCK(&mutex_cola_exec);
                list_add(cola_exec, query_candidata);
            UNLOCK(&mutex_cola_exec);
        } else {
            //TODO: Manejar error; cancelar ciclo?
        }
        
        log_info(get_logger(), 
                "Desalojo completado: Query %d ejecutándose, Query %d en READY (PC=%d)",
                query_candidata->query->query_id,
                query_victima->query->query_id,
                pc_recibido);
        
        
        continue;
    }
    
    UNLOCK(&mutex_estado_critico);
}



// Obtener query más prioritaria, en caso de empate el más antiguo (FIFO)
t_elemento_cola* obtener_query_mas_prioritaria_mas_antigua() {
    LOCK(&mutex_cola_ready);
    
    if (list_is_empty(cola_ready)) {
        UNLOCK(&mutex_cola_ready);
        return NULL;
    }
    
    t_elemento_cola* mejor = list_get(cola_ready, 0);
    
    for (int i = 1; i < list_size(cola_ready); i++) {
        t_elemento_cola* actual = list_get(cola_ready, i);
        
        if (actual->prioridad_efectiva < mejor->prioridad_efectiva) {
            mejor = actual;
        }
        
        else if (actual->prioridad_efectiva == mejor->prioridad_efectiva &&
                 actual->tiempo_llegada < mejor->tiempo_llegada) {
            mejor = actual;
        }
    }
    
    UNLOCK(&mutex_cola_ready);
    return mejor;
}


t_elemento_cola* obtener_victima_desalojo(uint32_t prioridad_desalojador) {
    LOCK(&mutex_cola_exec);
    
    if (list_is_empty(cola_exec)) {
        UNLOCK(&mutex_cola_exec);
        return NULL;
    }
    
    t_elemento_cola* victima = NULL;
    
    for (int i = 0; i < list_size(cola_exec); i++) {
        t_elemento_cola* actual = list_get(cola_exec, i);
        
        
        if (actual->prioridad_efectiva > prioridad_desalojador) {
            if (victima == NULL) {
                victima = actual;
            }
            
            else if (actual->prioridad_efectiva > victima->prioridad_efectiva) {
                victima = actual;
            }
            
            else if (actual->prioridad_efectiva == victima->prioridad_efectiva &&
                     actual->tiempo_llegada < victima->tiempo_llegada) {
                victima = actual;
            }
        }
    }
    
    UNLOCK(&mutex_cola_exec);
    return victima;
}

// Insertar ordenado: primero por prioridad, luego por antigüedad
void agregar_query_ordenada(t_list* lista, t_elemento_cola* elemento) {
    
    int posicion = list_size(lista);
    
    for (int i = 0; i < list_size(lista); i++) {
        t_elemento_cola* actual = list_get(lista, i);
        
        if (elemento->prioridad_efectiva < actual->prioridad_efectiva) {
            posicion = i;
            break;
        }
        else if (elemento->prioridad_efectiva == actual->prioridad_efectiva &&
                 elemento->tiempo_llegada < actual->tiempo_llegada) {
            posicion = i;
            break;
        }
    }
    
    list_add_in_index(lista, posicion, elemento);
    elemento->tiempo_llegada = timestamp_actual_en_milisegundos();
}

//TODO
uint32_t solicitar_desalojo_bloqueante(t_worker_conectado* worker_a_desalojar, uint32_t query_id) {
    return 0;
}





/*-------------------------------AGING-----------------------------------*/

void* main_aging(void* args) {
    tiempo_aging_ms = config_get_int_value(get_config(), "TIEMPO_AGING");
    
    if (tiempo_aging_ms == 0) {
        log_info(get_logger(), "Aging deshabilitado (TIEMPO_AGING = 0)");
        return NULL;
    }
    
    log_info(get_logger(), "Aging habilitado: intervalo de %d ms", tiempo_aging_ms);

    int cant_veces_aging_loop = 0;
    
    while (true) {
        
        dormir_milisegundos(tiempo_aging_ms);
        
        bool puede_desalojar_ahora = aplicar_aging_inteligente();
        
        if (puede_desalojar_ahora) {
            log_info(get_logger(), "Se inició evento de Aging");
            enviar_evento_planificacion(EVENTO_AGING_OCURRIDO, 
                                       VALOR_NULO_EVENTO, 
                                       VALOR_NULO_EVENTO, 
                                       VALOR_NULO_EVENTO);
        }
        cant_veces_aging_loop++;
        log_info(get_logger(), "Veces loop: %d", cant_veces_aging_loop);
    }
    return NULL;
}

/*
Separa en segundos y microsegundos para la estructura timeval.
EJ: 2500 milisegundos serán 2 tv_sec y 500000 tv_usec
*/
void dormir_milisegundos(int tiempo_aging_ms) {
    struct timeval tv;
    tv.tv_sec = tiempo_aging_ms / 1000;
    tv.tv_usec = (tiempo_aging_ms % 1000) * 1000;
    select(0, NULL, NULL, NULL, &tv);

}

bool aplicar_aging_inteligente() {
    LOCK(&mutex_estado_critico);
    
    // Obtener la MENOR prioridad (número mayor) en ejecución
    uint32_t umbral_desalojo = UINT32_MAX;
    
    LOCK(&mutex_cola_exec);
    if (!list_is_empty(cola_exec)) {
        for (int i = 0; i < list_size(cola_exec); i++) {
            t_elemento_cola* en_exec = list_get(cola_exec, i);
            if (en_exec->prioridad_efectiva < umbral_desalojo) {
                umbral_desalojo = en_exec->prioridad_efectiva;
            }
        }
    }
    UNLOCK(&mutex_cola_exec);
    
    bool puede_desalojar_ahora = false;
    uint64_t ahora = timestamp_actual_en_milisegundos();
    
    LOCK(&mutex_cola_ready);
    for (int i = 0; i < list_size(cola_ready); i++) {
        t_elemento_cola* elemento = list_get(cola_ready, i);
        
        uint64_t tiempo_desde_ultimo = ahora - elemento->ultimo_aging;
        uint32_t intervalos_nuevos = tiempo_desde_ultimo / tiempo_aging_ms;
        
        if (intervalos_nuevos > 0) {
            uint32_t prioridad_anterior = elemento->prioridad_efectiva;
            uint32_t nueva_prioridad = prioridad_anterior;
            
            if (nueva_prioridad >= intervalos_nuevos) {
                nueva_prioridad -= intervalos_nuevos;
            } else {
                nueva_prioridad = 0;
            }
            
            if (nueva_prioridad != prioridad_anterior) {
                elemento->prioridad_efectiva = nueva_prioridad;
                elemento->ultimo_aging = ahora;
                
                // Verificar si CRUZÓ el umbral de desalojo
                bool antes_no_podia = (prioridad_anterior >= umbral_desalojo);
                bool ahora_si_puede = (nueva_prioridad < umbral_desalojo);
                
                if (antes_no_podia && ahora_si_puede) {
                    log_info(get_logger(), 
                            "Aging RELEVANTE: Query %d (%d->%d) ahora puede desalojar (umbral=%d)", 
                            elemento->query->query_id,
                            prioridad_anterior,
                            nueva_prioridad,
                            umbral_desalojo);
                    puede_desalojar_ahora = true;
                } else {
                    log_debug(get_logger(), 
                            "Aging: Query %d prioridad %d -> %d (sin impacto)", 
                            elemento->query->query_id,
                            prioridad_anterior,
                            nueva_prioridad);
                }
            }
        }
    }
    UNLOCK(&mutex_cola_ready);
    
    UNLOCK(&mutex_estado_critico);
    
    return puede_desalojar_ahora;
}

void verificar_y_aplicar_aging_si_corresponde() {
    
    if (tiempo_aging_ms == 0) {
        return;
    }
    
    uint64_t ahora = timestamp_actual_en_milisegundos();
    
    LOCK(&mutex_cola_ready);
    for (int i = 0; i < list_size(cola_ready); i++) {
        t_elemento_cola* elemento = list_get(cola_ready, i);
        
        uint64_t tiempo_desde_ultimo = ahora - elemento->ultimo_aging;
        uint32_t intervalos_nuevos = tiempo_desde_ultimo / tiempo_aging_ms;
        
        if (intervalos_nuevos > 0) {
            uint32_t nueva_prioridad = elemento->prioridad_efectiva;
            
            if (nueva_prioridad >= intervalos_nuevos) {
                nueva_prioridad -= intervalos_nuevos;
            } else {
                nueva_prioridad = 0;
            }
            
            if (nueva_prioridad != elemento->prioridad_efectiva) {
                log_debug(get_logger(), 
                         "Aging durante desalojo: Query %d %d -> %d", 
                         elemento->query->query_id,
                         elemento->prioridad_efectiva,
                         nueva_prioridad);
                elemento->prioridad_efectiva = nueva_prioridad;
                elemento->ultimo_aging = ahora;
            }
        }
    }
    UNLOCK(&mutex_cola_ready);
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
                manejar_query_control_desconectado(evento->query_id);
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

            case EVENTO_AGING_OCURRIDO:
                // Aumento de prioridad por aging, despertar planificador
                log_debug(get_logger(), "Aging relevante ocurrió, reevaluando");
                despertar_planificador = true;
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

void enviar_evento_planificacion(t_tipo_evento tipo, uint32_t worker_id, uint32_t query_id, uint32_t query_pc) {
    t_evento_planificacion* evento = malloc(sizeof(t_evento_planificacion));
    evento->tipo = tipo;
    evento->worker_id = worker_id;
    evento->query_id = query_id;
    evento->program_counter = query_pc;

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
    
    // Marcar worker como desconectado y hacer wait a la cantidad de workers libres
    marcar_worker_desconectado(worker_id);
    sem_wait(cant_workers_libres);

    UNLOCK(&mutex_estado_critico);
    
}

void manejar_query_control_desconectado(uint32_t query_id_activo) {

    LOCK(&mutex_estado_critico);

    log_info(get_logger(), "Query Control %d desconectado", query_id_activo);
    
    if (query_id_activo >= 0) {
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
                
                solicitar_desalojo_bloqueante(worker_a_desalojar, query_id_activo);
            
                
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

    establecer_worker_desalojado(worker_id);
    
    UNLOCK(&mutex_estado_critico);
    
    enviar_evento_planificacion(EVENTO_WORKER_LIBERADO, worker_id, query_id, pc);
}