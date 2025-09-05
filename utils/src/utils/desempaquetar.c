#include "desempaquetar.h"



uint32_t extraer_uint32(void* stream, int* desplazamiento) {
    uint32_t value;
    // copio los 4 bytes al value respetando alineamiento
    memcpy(&value, (char*)stream + *desplazamiento, sizeof(uint32_t));
    *desplazamiento += sizeof(uint32_t);
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