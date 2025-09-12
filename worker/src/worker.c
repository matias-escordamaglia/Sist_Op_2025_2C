#include <worker.h>


char* archivo_config;
uint32_t id_worker;
int block_size;

pthread_t hilo_master;
pthread_t hilo_storage;

int main(int argc, char** argv)
{	

	if (argc < 3) {
        printf("Uso correcto: %s <archivo_config[path]> <ID_Worker[int]>\n", argv[0]);
        return EXIT_FAILURE;
    }

	archivo_config = argv[1];
    id_worker = atoi(argv[2]);

	config = iniciar_config(logger, archivo_config);

	log_level = obtener_log_level_config(config);

	logger = log_create("worker.log", "WORKER", true, log_level);
	
    ip_storage = config_get_string_value(config, "IP_STORAGE");
	puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");


	ip_master = config_get_string_value(config, "IP_MASTER");
	puerto_master = config_get_string_value(config, "PUERTO_MASTER");

    conexion_storage = crear_conexion(ip_storage, puerto_storage, logger);

    if (conexion_storage == -1) {
        log_error(logger, "No se pudo establecer conexión con STORAGE. Abortando.");
        terminar_programa(conexion_storage, -99, logger, config);
        exit(EXIT_FAILURE);
    }
    

    handshake_con_identificador_worker(conexion_storage, 1, id_worker, logger, "STORAGE");
    recv(conexion_storage, &block_size, sizeof(int), MSG_WAITALL);

	conexion_master = crear_conexion(ip_master, puerto_master, logger);

    if (conexion_master == -1) {
        log_error(logger, "No se pudo establecer conexión con el MASTER. Abortando.");
        terminar_programa(conexion_master, conexion_storage, logger, config);
        exit(EXIT_FAILURE);
    }
    
    handshake_con_identificador_worker(conexion_master, 1, id_worker, logger, "MASTER");

    int* server_fd_copia_storage = malloc(sizeof(int));
    *server_fd_copia_storage = conexion_storage;
	pthread_create(&hilo_storage, NULL, manejar_storage, server_fd_copia_storage);
	
    int* server_fd_copia_master = malloc(sizeof(int));
    *server_fd_copia_master = conexion_master;
    pthread_create(&hilo_master, NULL, manejar_master, server_fd_copia_master);

	/*
	Lo siguiente debe ajustarse para cada modulo
	*/
	pthread_join(hilo_storage, NULL);
	pthread_join(hilo_master, NULL);



    return 0;
}

void* manejar_storage(void* arg) {
    int conexion = *((int*)arg);
    free(arg);
    
    log_info(logger, " Worker - STORAGE conectado  - FD del socket: %d", conexion);

    while (1) {	
        int cod_op = recibir_operacion(conexion, logger);
        if (cod_op == -1) {
            log_info(logger, "STORAGE desconectado");
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                int size;
                void* buffer = recibir_buffer(&size, conexion);
                if (buffer == NULL) {
                    log_error(logger, "Error al recibir el buffer de STORAGE");
                    return NULL;
                }
                

                break;

            default:
                log_warning(logger, "Código de operación desconocido de QUERY: %d", cod_op);
                break;
        }
    }

    close(conexion);
    return NULL;
}

void* manejar_master(void* arg) {
    int conexion = *((int*)arg);
    free(arg);
    
    log_info(logger, " Worker - MASTER conectado  - FD del socket: %d", conexion);

    while (1) {
        int cod_op = recibir_operacion(conexion, logger);
        if (cod_op == -1) {
            log_info(logger, "MASTER desconectado");
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                int size;
                void* buffer = recibir_buffer(&size, conexion);
                if (buffer == NULL) {
                    log_error(logger, "Error al recibir el buffer de MASTER");
                    return NULL;
                }
                
                t_pedido_master_worker* pedido = desempaquetar_pedido_master_worker(buffer);
                
                if (!pedido) {
                    log_error(logger, "Error al desempaquetar pedido de MASTER");
                    free(buffer);
                    break;
                }

                t_motivo_pedido_master_worker motivo = pedido->motivo;
                char* path_query = pedido->query_path;
                uint32_t pc = pedido->program_counter;
                uint32_t qid = pedido->query_id;

                log_info(logger, "Nuevo pedido de Query. Query ID: %d - Path: %s - Program Count: %d - Motivo: %d " 
                                            , qid, path_query, pc, motivo);

                char* mensaje  = " holis"; 
                t_tipo_aviso_worker_master tipo_aviso = NUEVA_LECTURA;
                t_paquete* paquete_resp = crear_paquete();
    
                insertar_variable_a_paquete(paquete_resp, &(tipo_aviso), sizeof(t_tipo_aviso_worker_master));
                insertar_string_a_paquete(paquete_resp, mensaje);
                enviar_paquete(paquete_resp,conexion);

                free(pedido->query_path);
                free(pedido);
                free(buffer); 

                break;
                
            default:
                log_warning(logger, "Código de operación desconocido de MASTER: %d", cod_op);
                break;
        }
    }

    close(conexion);
    return NULL;
}

void handshake_con_identificador_worker(int socket, int valor ,uint32_t id_worker, t_log* logger, char* nombre_modulo) {
    if (handshake(socket, valor, logger, nombre_modulo) == (uint32_t)-1) {
        log_error(logger, "Handshake fallido con %s", nombre_modulo);
        exit(EXIT_FAILURE);
    }

    t_estado_handshake estado_handshake;

    send(socket, &id_worker, sizeof(uint32_t), 0);

    if (recv(socket, &estado_handshake, sizeof(t_estado_handshake), MSG_WAITALL) <= 0) {
        log_error(logger, "No se recibió respuesta de %s tras enviar el ID de WORKER", nombre_modulo);
        exit(EXIT_FAILURE);
    }
        

    if (estado_handshake == HANDSHAKE_OK) {
        log_info(logger, "WORKER %u registrado correctamente en %s", id_worker, nombre_modulo);
    } else {
        log_error(logger, "WORKER %u ya estaba registrado en %s. Abortando...", id_worker, nombre_modulo);
        exit(EXIT_FAILURE);
    }
}

void terminar_programa(int conexion1, int conexion2, t_log* logger, t_config* config) {
    
	log_info(logger, "Finalizando programa...");
    
	log_destroy(logger);
    config_destroy(config);
    
	close(conexion1);
	if(conexion2 != -99 ) {
		close(conexion2);
	}
	
}