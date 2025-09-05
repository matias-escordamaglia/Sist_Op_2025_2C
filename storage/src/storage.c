#include "storage.h"

t_log* logger;
t_config* config;

int server_fd_general;

pthread_t hilo_manejo_worker;

int main(int argc, char** argv) {
    
    
    config = iniciar_config(logger, "storage.config");

	log_level = obtener_log_level_config(config);

	logger = log_create("storage.log", "STORAGE", true, log_level);


    pasar_logger_a_manejo_worker(logger);
    

    char* puerto_general = config_get_string_value(config, "PUERTO_ESCUCHA");

    server_fd_general = iniciar_servidor(NULL, puerto_general, logger);
    if (server_fd_general == -1) {
        log_error(logger, "No se pudo iniciar el servidor general. Terminando.");
        return EXIT_FAILURE;
    }
    
    
    int* server_fd_copy = malloc(sizeof(int));
    *server_fd_copy = server_fd_general;
    pthread_create(&hilo_manejo_worker, NULL, manejar_cliente_worker, server_fd_copy);
    



    pthread_join(hilo_manejo_worker, NULL);
    

    close(server_fd_general);
    log_info(logger, "Servidor general cerrado correctamente.");
    

    
    return EXIT_SUCCESS;
}
