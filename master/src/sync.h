#ifndef SYNC_H_
#define SYNC_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <semaphore.h>
#include <commons/log.h>
#include <commons/config.h>


#define LOCK(mtx) pthread_mutex_lock(&(mtx))
#define UNLOCK(mtx) pthread_mutex_unlock(&(mtx))

typedef struct {
    t_log* logger;
    t_config* config;
} t_master;

typedef struct {
    int socket_fd;
    int tipo_cliente;
} t_conexion_identificada;

typedef enum {
    READY,
    EXEC,
	EXIT
} estado_query;


typedef struct {
    uint32_t query_id;
    uint32_t prioridad;
    uint32_t program_count;
    char* query_path;
} t_query;


extern sem_t* cant_queries_en_ready;
extern sem_t* cant_queries_en_exec;
extern sem_t* cant_queries_en_exit;

extern sem_t* cant_workers_libres;

extern sem_t* sem_envio_query_pendiente;

void iniciar_master_state(t_log* logger, t_config* config);
t_log* get_logger();
t_config* get_config();

void iniciar_semaforos();
void destruir_semaforos();
int obtener_valor_semaforo(sem_t* semaforo);

#endif /*SYNC_H_*/