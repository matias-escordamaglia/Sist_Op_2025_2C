#include "queries.h"

t_list* lista_queries;

static uint32_t contador_qid = 0;

t_query* crear_query(char* query_path, uint32_t prioridad, int conexion) {

    if (lista_queries == NULL)
        lista_queries = list_create();

    t_query* query = malloc(sizeof(t_query));
	query->query_id = establecer_siguiente_valor_qid();
    query->prioridad = prioridad;
	query->program_count = 0;
    query->query_path = string_duplicate(query_path);
    query->conexion = conexion;

    list_add(lista_queries, query);

    int grado_multiprocesamiento = cant_workers_conectados();
    log_info(get_logger(), "## Se conecta un Query Control para ejecutar la Query %s con prioridad %u - " 
                "Id asignado: %u. Nivel multiprocesamiento %u",
                query->query_path, query->prioridad, query->query_id, grado_multiprocesamiento);

    

	return query;
}

uint32_t establecer_siguiente_valor_qid() {
    return contador_qid++;
}

t_query* obtener_query_por_id_uso_interno(uint32_t id_query) {
    
    t_query* encontrado = NULL;
    for (int i = 0; i < list_size(lista_queries); i++) {
        t_query* query = list_get(lista_queries, i);
        if (query->query_id == id_query) {
            encontrado = query;
            break;
        }
    }
    
    return encontrado;
}

int conexion_de_query_por_id(uint32_t id_query) {
    t_query* query = obtener_query_por_id_uso_interno(id_query);
    return query->conexion;
}