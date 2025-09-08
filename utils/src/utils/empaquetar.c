#include "empaquetar.h"


/*
Se encarga de insertar una variable, manejando el tamanio del buffer junto a lo que inserta
*/
void insertar_variable_a_paquete(t_paquete* paquete, void* valor, int tamanio) {
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio);

	memcpy(paquete->buffer->stream + paquete->buffer->size, valor, tamanio);

	paquete->buffer->size += tamanio;
	
}

void insertar_string_a_paquete(t_paquete* paquete, char* string) {
    uint32_t longitud = strlen(string) + 1;
    insertar_variable_a_paquete(paquete, &longitud, sizeof(uint32_t));
    insertar_variable_a_paquete(paquete, string, longitud);
}

void insertar_uint32_a_paquete(t_paquete* paquete, uint32_t valor) {
    insertar_variable_a_paquete(paquete, &valor, sizeof(uint32_t));
}


t_paquete* empaquetar_para_prueba_conexion(t_prueba_conexion* prueba) {
    t_paquete* paquete = crear_paquete();

    insertar_uint32_a_paquete(paquete, prueba->numeroA);
    insertar_uint32_a_paquete(paquete, prueba->numeroB);

    insertar_string_a_paquete(paquete, prueba->string);

    //insertar_variable_a_paquete(paquete, &(prueba->motivo), sizeof(t_motivo_prueba));

    return paquete;
}

t_paquete* empaquetar_pedido_query_master(t_pedido_query_master* pedido) {
    t_paquete* paquete = crear_paquete();
    
    insertar_variable_a_paquete(paquete, &(pedido->tipo), sizeof(t_tipo_mensaje_query));

    insertar_uint32_a_paquete(paquete, pedido->prioridad);

    insertar_string_a_paquete(paquete, pedido->path_query);

    return paquete;
}