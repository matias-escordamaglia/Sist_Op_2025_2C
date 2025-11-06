#include "manejo-worker.h"

#include "operaciones.h"  
#include "storage.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <commons/crypto.h>

#include "./utils/desempaquetar.h"
#include "./utils/empaquetar.h"


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
                int offset = 0; 

                void* buffer_st = recibir_buffer(&size, cliente_fd);
                log_info(logger_worker, "[WORKER] Se recibe paquete desde WORKER %u", id_worker);
                Operation operation = extraer_operacion(buffer_st, &offset); 
                
                    char* nombre_file = extraer_string(buffer_st,&offset); 
                    char* nombre_tag  = extraer_string(buffer_st,&offset);

                    log_info(logger_worker, "Aplicando RETARDO_OPERACION para OP: %d", operation);
                    usleep(RETARDO_OPERACION * 1000);

                    int estado = -1; 

                    switch (operation){
                        case  CREATE:
                            estado = atender_create(nombre_file,nombre_tag);
                            break;
                        case  TRUNCATE:
                            int tamanio = (int)extraer_uint32(buffer_st,&offset);
                            estado = atender_truncate(nombre_file,nombre_tag,tamanio); 
                            break;      
                        case TAG: 
                            char* file_destino = extraer_string(buffer_st,&offset); 
                            char* tag_destino  = extraer_string(buffer_st,&offset);
                            estado = atender_tag(nombre_file,nombre_tag,file_destino,tag_destino); 
                            free(file_destino);
                            free(tag_destino);
                            break;
                        case WRITE:
                            break;
                        case READ: 
                            break;
                        case COMMIT:
                            //int estado = gestionar_commit(file,tag);
                            break;
                        case DELETE: 
                            break;
                        case END: 
                            break;
                        default:
                            break;
                        }
                    enviar_estado_op(estado,cliente_fd);

                    free(nombre_file);
                    free(nombre_tag);

                 free(buffer_st); 
            break;

            default:
                log_warning(logger_worker, "[WORKER] Código desconocido desde WORKER %u", id_worker);
                break;
        }
    }


    close(cliente_fd);
    return NULL;
}


Operation extraer_operacion(void* buffer_st, int* offset){
    Operation op; 
    memcpy(&op, buffer_st + *offset,sizeof(Operation));
    *offset += sizeof(Operation); 
    return op; 
} 
void enviar_estado_op(int estado, int socket){
    t_paquete* paquete = crear_paquete();
    insertar_int_a_paquete(paquete,estado);
    enviar_paquete(paquete,socket);
}
int atender_create(char* file, char* tag){
    char* key_file_tag = crear_key_file_tag(file,tag);  
    int estado; 
    pthread_mutex_lock(&mutex_diccionary);

    if (dictionary_has_key(file_tag_dic, key_file_tag)){
        log_error(logger, "Error: Se intentó operar sobre un File:Tag Existente: %s", key_file_tag);
        free(key_file_tag);
        return -1; 
    }else {
        estado = create(file,tag);
        if(estado==0){
            iniciar_mutex_file_tag(key_file_tag);
            log_info(logger,"File:Tag creado exitosamente: %s", key_file_tag);

        }else{
            log_info(logger, "Error el crear File:Tag->%s",key_file_tag); 
            return estado; 
        }
        
    }
    pthread_mutex_unlock(&mutex_diccionary); 
    free(key_file_tag);
    return estado; 
}
int atender_truncate(char* file, char* tag,int tamanio){
    char* key_file_tag = crear_key_file_tag(file,tag); 

    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);


    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        free(key_file_tag);
        return -1; 
    } 

    pthread_mutex_lock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 

    int estado_tag = obtener_estado_file_tag(key_file_tag); 
    int estado_truncate;

    if (estado_tag == 0 ) { //commited 
        log_error(logger, "Error: Se intentó TRUNCATE en un File:Tag en estado COMMITED: %s", key_file_tag);
        estado_truncate = -1; 
    } else {
        estado_truncate = truncar_archivo(file, tag, tamanio);
    }

    pthread_mutex_unlock(mutex_file_tag);

    free(key_file_tag);

    return estado_truncate; 
}

int atender_commit(char* file, char* tag){
    char* key_file_tag = crear_key_file_tag(file,tag);  

    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        free(key_file_tag);
        return -1; 
    } 
    pthread_mutex_lock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 

    if (obtener_estado_file_tag(key_file_tag) == 0) {
        log_warning(logger, "Warning: Se intentó COMMIT sobre un tag ya commiteado: %s", key_file_tag);
        pthread_mutex_unlock(mutex_file_tag);
        free(key_file_tag);
        return -1; ///no es error pero no se puedo commitear 
    }
    int estado = commit_tag(file,tag);
    int estado_dic ;
    if(estado==0){
        estado_dic = actualizar_dicc_estado(key_file_tag,0); 
    }
    pthread_mutex_unlock(mutex_file_tag);
    if(estado==0 && estado_dic == 0)
        return 0; 
   return -1; 
}
int atender_tag(char* file, char* tag, char* file_destino,char* tag_destino){
    char* key_file_tag = crear_key_file_tag(file,tag); 
    char* key_file_tag_destino = crear_key_file_tag(file_destino,tag_destino);
    char* file_tag_origen = add_seg_ruta(file,tag); 
    char* file_tag_destino = add_seg_ruta(file_destino,tag_destino); 
    pthread_mutex_lock(&mutex_diccionary); 
    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);
    
    if(dictionary_has_key(file_tag_dic,key_file_tag_destino)==true){
        log_error(logger, "Error: Se intentó operar sobre un File:Tag Destino existente: %s", key_file_tag_destino);
        free(key_file_tag);
        free(key_file_tag_destino);
        free(file_tag_origen);
        free(file_tag_destino); 
        pthread_mutex_unlock(&mutex_diccionary); 
        return -1; 
    }
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        free(key_file_tag);
        free(key_file_tag_destino);
        free(file_tag_origen);
        free(file_tag_destino); 
        pthread_mutex_unlock(&mutex_diccionary); 
        return -1; 
    } 
    pthread_mutex_lock(mutex_file_tag);

    int estado = commit_tag(file_tag_origen,file_tag_destino);

    iniciar_mutex_file_tag(key_file_tag_destino); 

    pthread_mutex_unlock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 
    free(key_file_tag);
    free(key_file_tag_destino);
    free(file_tag_origen);
    free(file_tag_destino); 


    return estado; 
}
