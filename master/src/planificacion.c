#include "planificacion.h"

t_list* cola_ready = NULL;
t_list* cola_exec = NULL;
t_list* cola_exit = NULL;

pthread_mutex_t mutex_cola_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exit = PTHREAD_MUTEX_INITIALIZER;

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

void crear_nuevo_query(char* query_path, uint32_t prioridad) {
    t_query* nuevo_query = crear_query(query_path, prioridad);

    // TODO REPETIDO 1 hilo con temporizador para aging posiblemente
    //inicializar_temporizador_query(nuevo_query);
    t_elemento_cola* nuevo_elemento = crear_nuevo_elemento(nuevo_query);

    LOCK(&mutex_cola_ready);
        list_add(cola_ready, nuevo_elemento);
        log_info(get_logger(), "Se crea nuevo Query con ID (%d) - Estado: READY", nuevo_query->query_id);
        sem_post(cant_queries_en_ready);
    UNLOCK(&mutex_cola_ready);
}

t_elemento_cola* crear_nuevo_elemento(t_query* query) {
    t_elemento_cola* nuevo_elemento = malloc(sizeof(t_elemento_cola));

    nuevo_elemento->query=query;
    //TODO REPETIDO 1: Aquí seguro vaya un temporizador u algún hilo para lo de aging si es que está activo
    nuevo_elemento->tiempo_llegada=timestamp_actual_en_milisegundos();

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

//TODO anañadirlo en master.c con algún hilo
void *iniciador_planificacion() {
    //Iniciar semaforos corto plazo -> iniciar_sem_cp();

    inicializar_listas_planificacion();

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
        sem_wait(cant_queries_en_ready);

        t_elemento_cola* mas_antiguo;

        LOCK(&mutex_cola_ready);
        mas_antiguo = obtener_mas_antiguo(cola_ready);
        if (mas_antiguo == NULL) {
            UNLOCK(&mutex_cola_ready);
            continue;
        }

        list_remove_element(cola_ready, mas_antiguo);
        UNLOCK(&mutex_cola_ready);

        // TODO Arreglar esto para cuando no haya workers libres/se deba desalojar
        t_worker_conectado* worker_libre = obtener_worker_libre();
        agregar_siguiente_query_a_enviar(mas_antiguo->query, worker_libre);
        
        LOCK(&mutex_cola_exec);
        list_add(cola_exec, mas_antiguo);
        UNLOCK(&mutex_cola_exit);
    }   
}


void planificar_por_prioridades() {

}