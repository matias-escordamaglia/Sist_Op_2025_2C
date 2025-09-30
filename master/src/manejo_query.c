#include "manejo_query.h"

void* manejar_query(void* arg) {
    t_conexion_identificada* conexion = (t_conexion_identificada*)arg;
    int cliente_fd = conexion->socket_fd;
    free(conexion);
    
    log_info(get_logger(), "## Master - QUERY conectado  - FD del socket: %d", cliente_fd);
    
    // Enviar confirmación de handshake
    uint32_t confirmacion = HANDSHAKE_OK;
    send(cliente_fd, &confirmacion, sizeof(uint32_t), 0);

    t_query* query;
    
    while (1) {
        int cod_op = recibir_operacion(cliente_fd, get_logger());
        if (cod_op == -1) {
            log_info(get_logger(), "QUERY desconectado, iniciando evento desconexión");
            t_worker_conectado* worker = obtener_worker_por_query_id(query->query_id);
            enviar_evento_planificacion(EVENTO_QUERY_CONTROL_DESCONECTADO, worker->id_worker, query->query_id, -1);
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                int size;
                void* buffer = recibir_buffer(&size, cliente_fd);
                if (buffer == NULL) {
                    log_error(get_logger(), "[QUERY] Error al recibir el buffer");
                    return NULL;
                }
                
				t_pedido_query_master* pedido = desempaquetar_pedido_query_master(buffer);
                
                if (!pedido) {
					log_error(get_logger(), "Error al desempaquetar pedido de QUERY");
                    free(buffer);
					break;
				}


                char* path_query = pedido->path_query;
                uint32_t prioridad = pedido->prioridad;

                log_info(get_logger(), "Nuevo pedido de Query. Path: %s - Prioridad: %d", path_query, prioridad);

                query = crear_nuevo_query(path_query, prioridad, cliente_fd);

                free(pedido->path_query);
                free(pedido);
                free(buffer);  
                
                break;
                
            default:
                log_warning(get_logger(), "Código de operación desconocido de QUERY: %d", cod_op);
                break;
        }
    }

    close(cliente_fd);
    return NULL;
}



bool mandar_lectura_a_query_con_id(char* string_crudo, uint32_t id_query) {

    
    char* file_tag;
    char* lectura;

    if (separar_string(string_crudo, &file_tag, &lectura)) {
        printf("FILE:TAG: %s\n", file_tag);
        printf("Lectura: %s\n", lectura);
       
    } else {
        printf("Error al separar el string\n");
        return false;
    }

    t_aviso_master_query* aviso_lectura = malloc(sizeof(t_aviso_master_query));

    t_query* query = obtener_query_por_id_uso_externo(id_query);

    aviso_lectura->motivo = LECTURA_QUERY;
    aviso_lectura->file_tag = file_tag;
    aviso_lectura->mensaje = lectura;

    t_paquete* paquete = empaquetar_aviso_master_query(aviso_lectura);
    if (!paquete) {
        log_error(get_logger(), "[MANEJO_QUERY] No se pudo empaquetar el aviso a query");
        return false;
    }

    enviar_paquete(paquete, query->conexion);

    log_info(get_logger(), "[MANEJO_QUERY] Aviso de lectura enviado a Query");

    //TODO pulir esto, revisar que cosas más se deben liberar

    // Liberar memoria
    free(file_tag);
    free(lectura);

    return true;
    
}


bool separar_string(char* input, char** file_tag, char** lectura) {
    char* espacio = strchr(input, ' ');
    
    // Checkea si hay un espacio en el string
    if (!espacio) {
        return false;
    }
    
    // Calcular longitudes
    int len_file_tag = espacio - input;
    int len_lectura = strlen(espacio + 1);
    
    // Alojar memoria
    *file_tag = malloc(len_file_tag + 1);
    *lectura = malloc(len_lectura + 1);
    
    if (!*file_tag || !*lectura) {
        // Error de memoria; se libera
        if (*file_tag) free(*file_tag);
        if (*lectura) free(*lectura);
        return false;
    }
    
    // Copiar las partes
    strncpy(*file_tag, input, len_file_tag);
    (*file_tag)[len_file_tag] = '\0';
    
    strcpy(*lectura, espacio + 1);
    
    return true;
}

void notificar_error_a_query_control(int conexion_query) {
    //TODO Realizar aviso de finalizacion por error; hacer polimorfico para finalizacion exitosa?
}

void notificar_finalizacion_a_query_control(uint32_t query_id) {
    //TODO Realizar aviso de finalizacion por error; hacer polimorfico para finalizacion exitosa?
}