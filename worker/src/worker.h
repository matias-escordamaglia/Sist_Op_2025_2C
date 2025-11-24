#ifndef WORKER_H_
#define WORKER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <readline/readline.h>
#include "queryInterpreter.h"
#include "utilsWorker.h"

// int ;
char* ip_storage;
char* puerto_storage;

char* ip_master;
char* puerto_master;

t_log_level log_level;

t_config* config;
t_log_level log_level;

void* manejar_storage(void* arg);
void* manejar_master(void* arg);
void handshake_con_identificador_worker(int socket, int valor, uint32_t id_cpu, t_log* logger, char* nombre_modulo);
void terminar_programa(int conexion1, int conexion2, t_log* logger, t_config* config);
t_programa* leer_y_partir(const char* path);
bool vacia_o_coment(const char* s);
void rstrip(char* s);
void cargar_scripts(const char* path_base, t_log* logger);

bool armar_y_enviar_confirmacion_a_master(t_aviso_worker_master* aviso_confirmacion, int conexion);


#endif /* WORKER_H_ */