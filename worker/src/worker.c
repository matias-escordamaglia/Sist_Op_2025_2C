#include <worker.h>


char* archivo_config;
uint32_t id_worker;

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

    conexion_storage = crear_conexion(ip_master, puerto_master, logger);

    if (conexion_storage == -1) {
        log_error(logger, "No se pudo establecer conexión con el MASTER. Abortando.");
        terminar_programa(conexion_storage, -99, logger, config);
        exit(EXIT_FAILURE);
    }
    
	handshake(conexion_storage, 1, logger, "WORKER");
	

    //Recién luego de que se conecta con storage se debe conectar con master 

	conexion_master = crear_conexion(ip_master, puerto_master, logger);

    if (conexion_master == -1) {
        log_error(logger, "No se pudo establecer conexión con el MASTER. Abortando.");
        terminar_programa(conexion_master, conexion_storage, logger, config);
        exit(EXIT_FAILURE);
    }
    
	handshake(conexion_master, HANDSHAKE_WORKER_MASTER, logger, "WORKER");

    
	pthread_create(&hilo_storage, NULL, manejar_storage, &conexion_storage);
	pthread_create(&hilo_master, NULL, manejar_master, &conexion_master);

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
    
    // Enviar confirmación de handshake
    uint32_t confirmacion = 0; // OK
    send(conexion, &confirmacion, sizeof(uint32_t), 0);
    

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

                t_list* lista = recibir_paquete(conexion, logger);
                if (lista == NULL || list_size(lista) == 0) {
                    log_error(logger, "[STORAGE] Error al recibir el paquete o paquete vacío");
                    return NULL;
                }
        
                //void* buffer = list_get(lista, 0);
                
                //Realizar cosas en caso que llegue un paquete

                list_destroy_and_destroy_elements(lista, free);
                
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
    
    // Enviar confirmación de handshake
    uint32_t confirmacion = 0; // OK
    send(conexion, &confirmacion, sizeof(uint32_t), 0);
    

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

                t_list* lista = recibir_paquete(conexion, logger);
                if (lista == NULL || list_size(lista) == 0) {
                    log_error(logger, "[MASTER] Error al recibir el paquete o paquete vacío");
                    return NULL;
                }
        
                //void* buffer = list_get(lista, 0);
                
                //Realizar cosas en caso que llegue un paquete

                list_destroy_and_destroy_elements(lista, free);
                
                break;
                
            default:
                log_warning(logger, "Código de operación desconocido de MASTER: %d", cod_op);
                break;
        }
    }

    close(conexion);
    return NULL;
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