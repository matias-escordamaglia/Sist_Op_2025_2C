#include "master.h"


pthread_t hilo_planificacion;
pthread_t hilo_pedidos_a_enviar_worker;
pthread_t hilo_principal;


int server_fd_general;


int main(int argc, char** argv) {
    
    char* path_config = argv[1];

    if (argc < 2) {
        printf("Uso correcto: %s <archivo_config[path]>\n", argv[0]);
        return EXIT_FAILURE;
    }

    config = iniciar_config(logger, path_config);

	log_level = obtener_log_level_config(config);

	logger = log_create("master.log", "master", true, log_level);

    iniciar_master_state(logger, config);
    
    iniciar_semaforos();
    iniciar_worker_manager();

    pthread_create(&hilo_pedidos_a_enviar_worker, NULL, tratar_siguientes_queries_a_enviar, NULL);
    pthread_detach(hilo_pedidos_a_enviar_worker);
    
    pthread_create(&hilo_planificacion, NULL, main_planificacion, NULL);
    pthread_detach(hilo_planificacion);

    
    char* puerto_escucha = config_get_string_value(get_config(), "PUERTO_ESCUCHA");

    server_fd_general = iniciar_servidor(NULL, puerto_escucha, get_logger());
    if (server_fd_general == -1) {
        log_error(get_logger(), "No se pudo iniciar el servidor de Master. Terminando.");
        return EXIT_FAILURE;
    }
    

    int* server_fd_copy = malloc(sizeof(int));
    *server_fd_copy = server_fd_general;
    pthread_create(&hilo_principal, NULL, manejar_conexiones_entrantes, server_fd_copy);
    
    // Esperar al hilo principal
    pthread_join(hilo_principal, NULL);
    

    close(server_fd_general);
    log_info(get_logger(), "Servidor general de Master cerrado correctamente.");
    

    destruir_semaforos();

    log_destroy(get_logger());
    return EXIT_SUCCESS;
}


void* manejar_conexiones_entrantes(void* arg) {
    int server_fd = *((int*)arg);
    free(arg);
    
    while(1) {
        int cliente_fd = esperar_cliente(server_fd, get_logger());
        if (cliente_fd == -1) {
            log_error(get_logger(), "Error al aceptar cliente");
            continue;
        }
        
        // Recibir identificación del cliente
        uint32_t tipo_cliente;
        int bytes_received = recv(cliente_fd, &tipo_cliente, sizeof(uint32_t), MSG_WAITALL);
        if (bytes_received <= 0) {
            log_error(get_logger(), "Error recibiendo identificación de cliente");
            close(cliente_fd);
            continue;
        }
        
        log_info(get_logger(), "Cliente conectado con identificación: %d", tipo_cliente);
        
        // Crear estructura para pasar al hilo correspondiente
        t_conexion_identificada* conexion = malloc(sizeof(t_conexion_identificada));
        conexion->socket_fd = cliente_fd;
        conexion->tipo_cliente = tipo_cliente;
        
        pthread_t hilo_cliente;
        
        switch(tipo_cliente) {
            case HANDSHAKE_WORKER_MASTER:
                log_info(get_logger(), "Conexión identificada como WORKER");
                pthread_create(&hilo_cliente, NULL, manejar_worker, conexion);
                pthread_detach(hilo_cliente);
                break;
                
            case HANDSHAKE_QUERY_MASTER:
                log_info(get_logger(), "Conexión identificada como QUERY");
                pthread_create(&hilo_cliente, NULL, manejar_query, conexion);
                pthread_detach(hilo_cliente);
                break;
                
            default:
                log_error(get_logger(), "Tipo de cliente desconocido: %d", tipo_cliente);
                close(cliente_fd);
                free(conexion);
                break;
        }
    }
    
    return NULL;
}   
