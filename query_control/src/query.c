#include <query.h>


char* archivo_config;
char* archivo_query;
uint32_t prioridad;

int main(int argc, char** argv)
{	

	if (argc < 4) {
        printf("Uso correcto: %s <archivo_config[path]> <archivo_query[path]> <prioridad[int]>\n", argv[0]);
        return EXIT_FAILURE;
    }

	archivo_config = argv[1];
	archivo_query = argv[2];
    prioridad = atoi(argv[3]);

	config = iniciar_config(logger, archivo_config);

	log_level = obtener_log_level_config(config);

	logger = log_create("query.log", "QUERY", true, log_level);
	

	ip = config_get_string_value(config, "IP_MASTER");
	puerto = config_get_string_value(config, "PUERTO_MASTER");

	log_info(logger, "IP_MASTER: %s", ip);
	log_info(logger, "PUERTO_MASTER: %s", puerto);

	conexion = crear_conexion(ip, puerto, logger);

    if (conexion == -1) {
        log_error(logger, "No se pudo establecer conexión con el MASTER. Abortando.");
        terminar_programa(conexion, logger, config);
        exit(EXIT_FAILURE);
    }
    
	handshake(conexion, HANDSHAKE_QUERY_MASTER, logger, "QUERY");

    //Enviar prioridad y query a master
	t_pedido_query_master* pedido_inicial = malloc(sizeof(t_pedido_query_master));
	pedido_inicial->tipo = QUERY_NUEVA_CONEXION;
	pedido_inicial->prioridad = prioridad;
	pedido_inicial->path_query = archivo_query;

	t_paquete* paquete = empaquetar_pedido_query_master(pedido_inicial);

	enviar_paquete(paquete, conexion);

	log_info(logger, "Pedido enviado. Path: %s - Prioridad: %d", pedido_inicial->path_query, pedido_inicial->prioridad);

	free(pedido_inicial);

	/*
	Posiblemente lo siguiente no deba ser un while, debe revisarse
	*/
	while (1) {
		int cod_op = recibir_operacion(conexion, logger);
		if (cod_op == -1) {
			log_error(logger, "MASTER se desconectó. Terminando QUERY.");
			break;
		}

		switch (cod_op) {
			case MENSAJE:
				recibir_mensaje(conexion, logger); 

				//Insertar Lógica de caso recepción de mensaje

				break;

			case PAQUETE: {
				log_info(logger, "[QUERY] Recibí un paquete desde MASTER");

				//Insertar Lógica de caso recepción de paquete

				break;
			}

			default:
				log_warning(logger, "Código de operación desconocido: %d", cod_op);
				break;
		}
	}



    return 0;
}

void terminar_programa(int conexion, t_log* logger, t_config* config) {
    
	log_info(logger, "Finalizando programa...");
    
	log_destroy(logger);
    config_destroy(config);
    
	close(conexion);
}
