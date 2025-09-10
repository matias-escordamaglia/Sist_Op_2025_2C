#include "sync.h"


t_master master_state;

sem_t* cant_queries_en_ready;
sem_t* cant_queries_en_exec;
sem_t* cant_queries_en_exit;

void iniciar_master_state(t_log* logger, t_config* config) {
    master_state.logger = logger;
    master_state.config = config;
}

t_log* get_logger() {
    return master_state.logger;
}

t_config* get_config() {
    return master_state.config;
}

void iniciar_semaforos() {
	
	cant_queries_en_ready = malloc(sizeof(sem_t)); 
    cant_queries_en_exec = malloc(sizeof(sem_t));
    cant_queries_en_exit =  malloc(sizeof(sem_t));

    sem_init(cant_queries_en_ready, 0, 0);
    sem_init(cant_queries_en_exec, 0, 0);
    sem_init(cant_queries_en_exit, 0, 0);

}	

void destruir_semaforos() {
	
	sem_destroy(cant_queries_en_ready);
    sem_destroy(cant_queries_en_exec);
    sem_destroy(cant_queries_en_exit);

}

int obtener_valor_semaforo(sem_t* semaforo) {
    int valor;
    if (sem_getvalue(semaforo, &valor) == -1) {
        return 99;
    }
    return valor;
}