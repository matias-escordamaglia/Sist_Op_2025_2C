#ifndef UTILS_DESEMPAQUETAR_H_ 
#define UTILS_DESEMPAQUETAR_H_ 

#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


uint32_t extraer_uint32(void* stream, int* desplazamiento);
char* extraer_string(void* stream, int* offset);

t_prueba_conexion* desempaquetar_prueba_conexionV1(void* stream);
t_prueba_conexion* desempaquetar_prueba_conexionV2(void* stream);

t_pedido_query_master* desempaquetar_pedido_query_master(void* stream);

#endif /* UTILS_DESEMPAQUETAR_H_  */