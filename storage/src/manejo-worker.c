#include "manejo-worker.h"

#include "operaciones.h"  
#include "storage.h"

t_log* logger_worker;
t_config* blockconfig = NULL;

void pasar_log_config_a_manejo_worker(t_log* l, t_config* c) {
    blockconfig = c; 
    logger_worker = l;
} 
void* manejar_cliente_worker(void* arg) {
    int server_fd = (*(int*)arg);
    free(arg);
    log_info(logger,"Esperando conexiones..."); 

    while (1) {
        int cliente_fd = esperar_cliente(server_fd, logger_worker);
        if (cliente_fd == -1) {
            log_error(logger_worker, "Error al aceptar cliente worker");
            continue;
        }

        pthread_t hilo_worker;
        int* fd_copia = malloc(sizeof(int));
        *fd_copia = cliente_fd;
        pthread_create(&hilo_worker, NULL, atender_conexion_worker, fd_copia);
        pthread_detach(hilo_worker);
    }

    return NULL;
}
void* atender_conexion_worker(void* arg) {
    int cliente_fd = *((int*)arg);
    free(arg);

    uint32_t id_worker;
    uint32_t respuesta;

    // Handshake inicial: debe ser 1
    int bytes = recv(cliente_fd, &respuesta, sizeof(uint32_t), MSG_WAITALL);
    if (bytes <= 0 || respuesta != 1) {
        log_error(logger_worker, "[WORKER] Error en handshake con WORKER. FD: %d", cliente_fd);
        t_estado_handshake error = HANDSHAKE_FALLO;
        send(cliente_fd, &error, sizeof(t_estado_handshake), 0);
        close(cliente_fd);
        return NULL;
    }

    t_estado_handshake ok = HANDSHAKE_OK;
    send(cliente_fd, &ok, sizeof(t_estado_handshake), 0);


    if (recv(cliente_fd, &id_worker, sizeof(uint32_t), MSG_WAITALL) <= 0) {
        log_error(logger_worker, "[WORKER] No se pudo recibir el ID del WORKER (FD %d)", cliente_fd);
        close(cliente_fd);
        return NULL;
    }
    //incluir en le hs el envio de datos .config a worker

    t_estado_handshake registrado = HANDSHAKE_OK;
    send(cliente_fd, &registrado, sizeof(t_estado_handshake), 0);
    log_info(logger_worker, "Worker ID: %u se conectó", id_worker); 
    
    
    char* blockSizeChar = config_get_string_value(blockconfig, "BLOCK_SIZE");
    int block_size = atoi(blockSizeChar); 

    log_info(logger_worker, "Enviando block_size=%d", block_size);
    send(cliente_fd, &block_size, sizeof(int), 0);

    

    // Bucle principal
    while (1) {
        int cod_op = recibir_operacion(cliente_fd, logger_worker);
        if (cod_op == -1) {
            log_warning(logger_worker, "[WORKER] WORKER %u se desconectó (FD %d)", id_worker, cliente_fd);
            break;
        }

        switch (cod_op) {
            case PAQUETE:
                int size; 
                void* buffer_st = recibir_buffer(&size, cliente_fd);
                log_info(logger_worker, "[WORKER] Se recibe paquete desde WORKER %u", id_worker);
                Operation operation = extraer_operacion(buffer_st); 
                    switch (operation)
                    {
                    case  CREATE:
                        //aca el desarrollo
                        break;
                    case  TRUNCATE:
                        //aca el desarrollo
                    case WRITE:
                        //aca el desarrollo

                        break;
                    case READ: 
                        break;
                    case TAG: 
                        break;
                    case COMMIT:
                        break;
                    case FLUSH:
                        break;
                    case DELETE: 
                        break;
                    case END: 
                        break;
                    default:
                        break;
                    }
                break;

            default:
                log_warning(logger_worker, "[WORKER] Código desconocido desde WORKER %u", id_worker);
                break;
        }
    }


    close(cliente_fd);
    return NULL;
}


Operation extraer_operacion(void* buffer_st){
    Operation op; 
    memcpy(&op,buffer_st,sizeof(Operation));
} 
