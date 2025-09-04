#include "manejo_worker.h"

void* manejar_worker(void* arg) {
    t_conexion_identificada* conexion = (t_conexion_identificada*)arg;
    int cliente_fd = conexion->socket_fd;
    free(conexion);
    
    log_info(get_logger(), "Master - WORKER conectado - FD del socket: %d", cliente_fd);
    
    // Enviar confirmación de handshake
    uint32_t confirmacion = 0; // OK
    send(cliente_fd, &confirmacion, sizeof(uint32_t), 0);
    

    while (1) {
        int cod_op = recibir_operacion(cliente_fd, get_logger());
        if (cod_op == -1) {
            log_info(get_logger(), "WORKER desconectado");
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                t_list* lista = recibir_paquete(cliente_fd, get_logger());
                if (lista == NULL || list_size(lista) == 0) {
                    log_error(get_logger(), "[WORKER] Error al recibir el paquete o paquete vacío");
                    return NULL;
                }
        
                //void* buffer = list_get(lista, 0);
                
                //Realizar cosas en caso que llegue un paquete

                list_destroy_and_destroy_elements(lista, free);
                
                break;
                
            default:
                log_warning(get_logger(), "Código de operación desconocido de WORKER: %d", cod_op);
                break;
        }
    }

    close(cliente_fd);
    return NULL;
}