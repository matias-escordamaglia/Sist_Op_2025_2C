#ifndef UTILS_EMPAQUETAR_H_ 
#define UTILS_EMPAQUETAR_H_ 

#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>



void insertar_variable_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void insertar_string_a_paquete(t_paquete* paquete, char* string);
void insertar_binario_a_paquete(t_paquete* paquete, char* string, int tamanio);
void insertar_bytes_a_paquete(t_paquete* paquete, void* datos, int tamanio);

void insertar_uint32_a_paquete(t_paquete* paquete, uint32_t valor);
void insertar_int_a_paquete(t_paquete* paquete, int valor);

int enviar_instruccion_a_storage(int conexion_storage, char* tag, int tamanio, int operacion);

t_paquete* empaquetar_para_prueba_conexion(t_prueba_conexion* prueba);
t_paquete* empaquetar_pedido_query_master(t_pedido_query_master* pedido);
t_paquete* empaquetar_aviso_master_query(t_aviso_master_query* aviso);
t_paquete* empaquetar_pedido_master_worker(t_pedido_master_worker* pedido);
t_paquete* empaquetar_aviso_worker_master(t_aviso_worker_master* aviso);
t_paquete* empaquetar_operacion_create(char* file, char* tag, uint32_t queryid);
t_paquete* empaquetar_operacion_truncate(char* file, char* tag, uint32_t tam, uint32_t queryid);
t_paquete* empaquetar_operacion_tag(char* file_origen, char* tag_origen, char* file_dest, char* tag_dest, uint32_t queryid);
t_paquete* empaquetar_operacion_commit(char* file, char* tag, uint32_t queryid);
t_paquete* empaquetar_operacion_delete(char* file, char* tag, uint32_t queryid);
t_paquete* empaquetar_operacion_end(uint32_t queryid);
t_paquete* empaquetar_operacion_fin_error(t_tipo_aviso_worker_master tipodeerror, char* error_code);

#endif /* UTILS_EMPAQUETAR_H_ */