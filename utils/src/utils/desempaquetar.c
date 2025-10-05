#include "desempaquetar.h"



uint32_t extraer_uint32(void* stream, int* desplazamiento) {
    uint32_t value;
    // copio los 4 bytes al value respetando alineamiento
    memcpy(&value, (char*)stream + *desplazamiento, sizeof(uint32_t));
    *desplazamiento += sizeof(uint32_t);
    return value;
}

int extraer_int(void* stream, int* desplazamiento) {
    uint32_t value;
   
    memcpy(&value, (char*)stream + *desplazamiento, sizeof(int));
    *desplazamiento += sizeof(int);

    return value;
}

char* extraer_string(void* stream, int* offset) {
    uint32_t longitud = extraer_uint32(stream, offset);
    char* string = malloc(longitud);
    memcpy(string, (char*)stream + *offset, longitud);
    *offset += longitud;
    return string;
}



t_prueba_conexion* desempaquetar_prueba_conexionV1(void* stream) {
    int offset = 0;
    t_prueba_conexion* recepcion_prueba = malloc(sizeof(t_prueba_conexion));

    memcpy(&(recepcion_prueba->numeroA), stream + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(&(recepcion_prueba->numeroB), stream + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint32_t len_nombre = extraer_uint32(stream, &offset);
    recepcion_prueba->string = calloc(len_nombre, sizeof(char));
    memcpy(recepcion_prueba->string, stream + offset, len_nombre);
    offset += len_nombre;

    /*memcpy(&(recepcion_prueba->motivo), stream + offset, sizeof(t_motivo_prueba));
    offset += sizeof(t_motivo_prueba);*/

    return recepcion_prueba;
}

t_prueba_conexion* desempaquetar_prueba_conexionV2(void* stream) {
    int offset = 0;
    t_prueba_conexion* recepcion_prueba = malloc(sizeof(t_prueba_conexion));
   
    recepcion_prueba->numeroA = extraer_uint32(stream, &offset);

    recepcion_prueba->numeroB = extraer_uint32(stream, &offset);

    recepcion_prueba->string = extraer_string(stream, &offset);

    /*memcpy(&(recepcion_prueba->motivo), stream + offset, sizeof(t_motivo_prueba));
    offset += sizeof(t_motivo_prueba);*/

    return recepcion_prueba;
}

t_pedido_query_master* desempaquetar_pedido_query_master(void* stream) {
    int offset = 0;
    t_pedido_query_master* recepcion_pedido = malloc(sizeof(t_pedido_query_master));

    recepcion_pedido->prioridad = extraer_uint32(stream, &offset);

    recepcion_pedido->path_query = extraer_string(stream, &offset);

    return recepcion_pedido;
}

t_aviso_master_query* desempaquetar_aviso_master_query(void* stream) {
    int offset = 0;
    t_aviso_master_query* recepcion_aviso = malloc(sizeof(t_aviso_master_query));

    memcpy(&(recepcion_aviso->motivo), stream + offset, sizeof(t_motivo_aviso_master_query));
    offset += sizeof(t_motivo_aviso_master_query);

    recepcion_aviso->file_tag = extraer_string(stream, &offset);

    recepcion_aviso->mensaje = extraer_string(stream, &offset);

    return recepcion_aviso;
}


t_pedido_master_worker* desempaquetar_pedido_master_worker(void* stream) {
    int offset = 0;
    t_pedido_master_worker* recepcion_pedido = malloc(sizeof(t_pedido_master_worker));

    memcpy(&(recepcion_pedido->motivo), stream + offset, sizeof(t_motivo_pedido_master_worker));
    offset += sizeof(t_motivo_pedido_master_worker);

    recepcion_pedido->query_id = extraer_uint32(stream, &offset);

    recepcion_pedido->program_counter = extraer_uint32(stream, &offset);

    recepcion_pedido->query_path = extraer_string(stream, &offset);

    return recepcion_pedido;
}

t_aviso_worker_master* desempaquetar_aviso_worker_master(void* stream) {
    int offset = 0;
    t_aviso_worker_master* recepcion_aviso = malloc(sizeof(t_aviso_worker_master));

    memcpy(&(recepcion_aviso->tipo_aviso), stream + offset, sizeof(t_tipo_aviso_worker_master));
    offset += sizeof(t_tipo_aviso_worker_master);

    recepcion_aviso->argumento = extraer_string(stream, &offset);

    return recepcion_aviso;
}