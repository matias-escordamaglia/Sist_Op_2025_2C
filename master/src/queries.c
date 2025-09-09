#include "queries.h"

t_list* lista_queries;

static uint32_t contador_qid = 0;

t_query* crear_query(char* query_path, uint32_t prioridad) {

    if (lista_queries == NULL)
        lista_queries = list_create();

    t_query* query = malloc(sizeof(t_query));
	query->query_id = establecer_siguiente_valor_qid();
    query->prioridad = prioridad;
	query->program_count = 0;
	query->estado = READY;
    query->query_path = string_duplicate(query_path);

    list_add(lista_queries, query);

	return query;
}

uint32_t establecer_siguiente_valor_qid() {
    return contador_qid++;
}