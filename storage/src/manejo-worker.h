#ifndef MANEJO_WORKER_H_
#define MANEJO_WORKER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"


extern t_config* blockconfig;

void pasar_log_config_a_manejo_worker(t_log* l, t_config*);
void* manejar_cliente_worker(void* arg);
void* atender_conexion_worker(void* arg);
Operation extraer_operacion(void*, int* );
void enviar_estado_op(int estado, int socket);
void enviar_paquete_read(int estado,char* contenido_salida, int tamanio_leido,int socket);
int atender_create(char* file, char* tag);
int atender_truncate(char* file, char* tag,int tamanio);
int atender_commit(char* file, char* tag);
int atender_tag(char* file, char* tag, char* file_destino,char* tag_destino);
int atender_escritura(char* file, char* tag, int bloque, char* contenido,int tam);
int atender_lectura(char* file, char* tag, int bloque_logico, int* tamanio_leido, char** contenido_salida);
int atender_delete(char* file, char* tag); 
const char* operation_to_string(Operation op);






#endif /* MANEJO_WORKER_H_ */