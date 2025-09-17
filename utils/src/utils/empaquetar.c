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

void insertar_int_a_paquete(t_paquete* paquete, int valor) {
    insertar_variable_a_paquete(paquete, &valor, sizeof(int));
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

t_paquete* empaquetar_pedido_master_worker(t_pedido_master_worker* pedido) {
    t_paquete* paquete = crear_paquete();
    
    insertar_variable_a_paquete(paquete, &(pedido->motivo), sizeof(t_motivo_pedido_master_worker));

    insertar_uint32_a_paquete(paquete, pedido->query_id);

    insertar_uint32_a_paquete(paquete, pedido->program_counter);

    insertar_string_a_paquete(paquete, pedido->query_path);

    return paquete;
}

t_paquete* empaquetar_aviso_worker_master(t_aviso_worker_master* aviso) {
    t_paquete* paquete = crear_paquete();

    insertar_variable_a_paquete(paquete, &(aviso->tipo_aviso), sizeof(t_tipo_aviso_worker_master));

    insertar_string_a_paquete(paquete, aviso->argumento);

    return paquete;
}

int enviar_instruccion_a_storage(int conexion_storage, char* tag, int tamanio, int operacion){
 
}    
