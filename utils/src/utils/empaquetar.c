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
void insertar_binario_a_paquete(t_paquete* paquete, char* string, int longitud){
    insertar_variable_a_paquete(paquete, &longitud, sizeof(int));
    insertar_variable_a_paquete(paquete, string, longitud);
}


void insertar_uint32_a_paquete(t_paquete* paquete, uint32_t valor) {
    insertar_variable_a_paquete(paquete, &valor, sizeof(uint32_t));
}

void insertar_int_a_paquete(t_paquete* paquete, int valor) {
    insertar_variable_a_paquete(paquete, &valor, sizeof(int));
}

void insertar_bytes_a_paquete(t_paquete* paquete, void* datos, int tamanio) {
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio);
    memcpy(paquete->buffer->stream + paquete->buffer->size, datos, tamanio);
    paquete->buffer->size += tamanio;
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

    insertar_uint32_a_paquete(paquete, pedido->prioridad);

    insertar_string_a_paquete(paquete, pedido->path_query);

    return paquete;
}

t_paquete* empaquetar_aviso_master_query(t_aviso_master_query* aviso) {
    t_paquete* paquete = crear_paquete();

    insertar_variable_a_paquete(paquete, &(aviso->motivo), sizeof(t_motivo_aviso_master_query));

    insertar_string_a_paquete(paquete, aviso->file_tag);

    insertar_string_a_paquete(paquete, aviso->mensaje);

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

t_paquete* empaquetar_operacion_create(char* file, char* tag, uint32_t query_id) {
    t_paquete* paquete = crear_paquete();

    insertar_uint32_a_paquete(paquete, CREATE);

    insertar_string_a_paquete(paquete, file);

    insertar_string_a_paquete(paquete, tag);
    
    insertar_uint32_a_paquete(paquete, query_id);
    return paquete;
}

t_paquete* empaquetar_operacion_truncate(char* file, char* tag, uint32_t tam, uint32_t queryid) {
    t_paquete* paquete = crear_paquete();
    
    insertar_uint32_a_paquete(paquete, TRUNCATE);

    insertar_string_a_paquete(paquete, file);

    insertar_string_a_paquete(paquete, tag);

    insertar_uint32_a_paquete(paquete, tam);   

    insertar_uint32_a_paquete(paquete, queryid);

    return paquete;
}

t_paquete* empaquetar_operacion_tag(char* file_origen, char* tag_origen, char* file_dest, char* tag_dest, uint32_t queryid){
    t_paquete* paquete = crear_paquete();
    
    insertar_uint32_a_paquete(paquete, TAG);

    insertar_string_a_paquete(paquete, file_origen);

    insertar_string_a_paquete(paquete, tag_origen);

    insertar_string_a_paquete(paquete, file_dest);

    insertar_string_a_paquete(paquete, tag_dest);

    insertar_uint32_a_paquete(paquete, queryid);

    return paquete;
}

t_paquete* empaquetar_operacion_fin_error(t_tipo_aviso_worker_master tipodeerror, char* error_code) {
    t_paquete* paquete = crear_paquete();
    insertar_uint32_a_paquete(paquete, FIN_ERROR);
    insertar_variable_a_paquete(paquete, &(tipodeerror), sizeof(t_tipo_aviso_worker_master));
    insertar_string_a_paquete(paquete, error_code);

    return paquete;
}

t_paquete* empaquetar_operacion_commit(char* file, char* tag, uint32_t queryid) {
    t_paquete* paquete = crear_paquete();

    insertar_uint32_a_paquete(paquete, COMMIT);
    insertar_string_a_paquete(paquete, file);
    insertar_string_a_paquete(paquete, tag);
    insertar_uint32_a_paquete(paquete, queryid);

    return paquete;
}

t_paquete* empaquetar_operacion_delete(char* file, char* tag, uint32_t queryid) {
    t_paquete* paquete = crear_paquete();

    insertar_uint32_a_paquete(paquete, DELETE);
    insertar_string_a_paquete(paquete, file);
    insertar_string_a_paquete(paquete, tag);
    insertar_uint32_a_paquete(paquete, queryid);

    return paquete;
}

t_paquete* empaquetar_operacion_end(uint32_t queryid) {
    t_paquete* paquete = crear_paquete();
    insertar_uint32_a_paquete(paquete, END);
    insertar_uint32_a_paquete(paquete, queryid);
    return paquete;
}