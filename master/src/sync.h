#ifndef SYNC_H_
#define SYNC_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>
#include <commons/log.h>
#include <commons/config.h>


typedef struct {
    t_log* logger;
    t_config* config;
} t_master;

typedef struct {
    int socket_fd;
    int tipo_cliente;
} t_conexion_identificada;

t_master master_state;

void iniciar_master_state(t_log logger, t_config config);
t_log* get_logger();
t_config* get_config();

void iniciar_semaforos();
void destruir_semaforos();
int obtener_valor_semaforo(sem_t* semaforo);

#endif /*SYNC_H_*/