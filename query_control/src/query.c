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

	if (prioridad < 0) {
		printf("La prioridad debe ser un valor mayor o igual a 0\n");
		return EXIT_FAILURE;
	}

	t_log* temp_logger = log_create("query.log", "QUERY", true, LOG_LEVEL_INFO);

	config = iniciar_config(temp_logger, archivo_config);

	log_level = obtener_log_level_config(config);
	log_destroy(temp_logger);
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

	log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %s", ip, puerto);

    //Enviar prioridad y query a master
	t_pedido_query_master* pedido_inicial = malloc(sizeof(t_pedido_query_master));
	pedido_inicial->prioridad = prioridad;
	pedido_inicial->path_query = archivo_query;

	t_paquete* paquete = empaquetar_pedido_query_master(pedido_inicial);

	enviar_paquete(paquete, conexion);

	log_info(logger, "## Solicitud de ejecución de Query: %s, prioridad: %d", pedido_inicial->path_query, pedido_inicial->prioridad);

	free(pedido_inicial);

	int continuar = 1;

	while (continuar) {
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

				int size;
                void* buffer = recibir_buffer(&size, conexion);
                if (buffer == NULL) {
                    log_error(logger, "Error al recibir el buffer");
                    return EXIT_FAILURE;
                }
                
				t_aviso_master_query* aviso = desempaquetar_aviso_master_query(buffer);
                
                if (!aviso) {
					log_error(logger, "Error al desempaquetar aviso de MASTER");
                    free(buffer);
					break;
				}

				switch (aviso->motivo)
				{
				case LECTURA_QUERY:
					log_info(logger, "## Lectura realizada: Archivo %s, contenido: %s", aviso->file_tag, aviso->mensaje);
					break;

				case QUERY_FINALIZADO:
					log_info(logger, "## Query Finalizada - %s", aviso->mensaje);
					continuar = 0;
					break;

				default:
					log_error(logger, "Motivo INEXISTENTE recibido de Master. Número de motivo: %u", aviso->motivo);
					break;
				}

				break;
			}

			default:
				log_warning(logger, "Código de operación desconocido: %d", cod_op);
				break;
		}
	}

	terminar_programa(conexion, logger, config);

    return EXIT_SUCCESS;
}

void terminar_programa(int conexion, t_log* logger, t_config* config) {
    
	log_info(logger, "Finalizando programa...");
    
	log_destroy(logger);
    config_destroy(config);
    
	close(conexion);
}
