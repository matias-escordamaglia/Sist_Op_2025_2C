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
Operation extraer_operacion(void* );


void create(char*, char*, char*);
void truncar_archivo(char*, char*, char*);
void tag_file(char*, char*);
void commit_tag();
void escritura_bloque();
void lectura_bloque();
void eliminar_tag();

int obtener_tamano(char*);
void incrementar(int, int);
void decrementar(int, int);
void copiar_archivo(char*, char*);
void copiar_directorio(char*, char*);

#endif /* MANEJO_WORKER_H_ */