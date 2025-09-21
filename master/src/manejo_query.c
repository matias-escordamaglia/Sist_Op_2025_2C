#include "manejo_query.h"

void* manejar_query(void* arg) {
    t_conexion_identificada* conexion = (t_conexion_identificada*)arg;
    int cliente_fd = conexion->socket_fd;
    free(conexion);
    
    log_info(get_logger(), "## Master - QUERY conectado  - FD del socket: %d", cliente_fd);
    
    // Enviar confirmación de handshake
    uint32_t confirmacion = HANDSHAKE_OK;
    send(cliente_fd, &confirmacion, sizeof(uint32_t), 0);
    

    while (1) {
        int cod_op = recibir_operacion(cliente_fd, get_logger());
        if (cod_op == -1) {
            log_info(get_logger(), "QUERY desconectado");
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

                crear_nuevo_query(path_query, prioridad, cliente_fd);

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