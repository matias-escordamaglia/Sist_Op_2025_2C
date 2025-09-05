#ifndef QUERY_H_
#define QUERY_H_

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

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"

int conexion;
char* ip;
char* puerto;
t_log_level log_level;

t_log* logger;
t_config* config;


void terminar_programa(int conexion, t_log* logger, t_config* config);

#endif /* QUERY_H_ */