#ifndef UTILS_EMPAQUETAR_H_ 
#define UTILS_EMPAQUETAR_H_ 

#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



void insertar_variable_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void insertar_string_a_paquete(t_paquete* paquete, char* string);
void insertar_uint32_a_paquete(t_paquete* paquete, uint32_t valor);
void insertar_int_a_paquete(t_paquete* paquete, int valor);

t_paquete* empaquetar_para_prueba_conexion(t_prueba_conexion* prueba);
t_paquete* empaquetar_pedido_query_master(t_pedido_query_master* pedido);
t_paquete* empaquetar_pedido_master_worker(t_pedido_master_worker* pedido);
t_paquete* empaquetar_aviso_worker_master(t_aviso_worker_master* aviso);

#endif /* UTILS_EMPAQUETAR_H_ */