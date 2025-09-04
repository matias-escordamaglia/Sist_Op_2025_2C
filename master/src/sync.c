#include "sync.h"

t_master master_state;

void iniciar_master_state(t_log logger, t_config config) {
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
	
	//Iniciar posibles semáforos

}	

void destruir_semaforos() {
	
	//Destruir los semáforos al finalizar todo el proceso de Kernel

}

int obtener_valor_semaforo(sem_t* semaforo) {
    int valor;
    if (sem_getvalue(semaforo, &valor) == -1) {
        return 99;
    }
    return valor;
}