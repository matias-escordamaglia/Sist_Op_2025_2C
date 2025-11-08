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
        printf("----------------------------------------------------------------------------------\n");
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

                    //log_info(logger_worker, "Aplicando RETARDO_OPERACION para OP: %d", operation);
                    usleep(RETARDO_OPERACION * 100);

                    int estado = -1; 
                    char* contenido_salida;
                    int tamanio_leido;
                    
                log_info(logger_worker, "EJECUTANDO operación: %s", operation_to_string(operation));

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
                        case COMMIT:
                            estado = atender_commit(nombre_file,nombre_tag);
                            break;
                        case WRITE:
                            int bloque = (int)extraer_int(buffer_st,&offset);
                            int tamanio_contenido ; 
                            char* contenido = extraer_binario_y_tamanio(buffer_st, &offset,&tamanio_contenido);
                            log_contenido_legible(logger_worker, "Contenido WRITE recibido", contenido, tamanio_contenido); 
                            estado = atender_escritura(nombre_file, nombre_tag,bloque,contenido,tamanio_contenido);
                            free(contenido); 
                            break;
                        case READ: 
                            int bloque_logico = extraer_int(buffer_st,&offset);
                            tamanio_leido=0; 
                            contenido_salida = NULL;
                            estado = atender_lectura(nombre_file,nombre_tag,bloque_logico,&tamanio_leido,&contenido_salida); 
                            break;
                        case DELETE: 
                            //estado = atender_delete(nombre_file,nombre_tag);
                            break;
                        default:
                            break;
                        }
                    if(operation==READ){
                        enviar_paquete_read(estado,contenido_salida,tamanio_leido,cliente_fd);
                        free(contenido_salida);
                    }
                    else
                        enviar_estado_op(estado,cliente_fd);

                    free(nombre_file);
                    free(nombre_tag);

                 free(buffer_st); 
                  if(estado==0)
                    log_info(logger_worker,"OPERACIÓN EXITOSA");
                    else
                    log_info(logger_worker,"OPERACIÓN NO EXITOSA");
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
void enviar_paquete_read(int estado,char* contenido_salida, int tamanio_leido,int socket){
    t_paquete* paquete = crear_paquete();
    insertar_int_a_paquete(paquete,estado);
    insertar_int_a_paquete(paquete,tamanio_leido);
    insertar_binario_a_paquete(paquete,contenido_salida,tamanio_leido);
    log_contenido_legible(logger, "Contenido READ leido", contenido_salida, tamanio_leido);
    enviar_paquete(paquete,socket);
}

int atender_create(char* file, char* tag){
    char* key_file_tag = crear_key_file_tag(file,tag);  
    int estado; 
    pthread_mutex_lock(&mutex_diccionary);

    if (dictionary_has_key(file_tag_dic, key_file_tag)){
        log_error(logger, "Error: Se intentó operar sobre un File:Tag Existente: %s", key_file_tag);
        free(key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary); 
        return -1; 
    }else {
        estado = create(file,tag);
        if(estado==0){
            iniciar_mutex_file_tag(key_file_tag);
            anadir_a_dicc_estado(key_file_tag); 
            log_info(logger,"File:Tag creado exitosamente: %s", key_file_tag);

        }else{
            log_info(logger, "Error el crear File:Tag->%s",key_file_tag); 
            pthread_mutex_unlock(&mutex_diccionary); 
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
        pthread_mutex_unlock(&mutex_diccionary);
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
        pthread_mutex_unlock(&mutex_diccionary);
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
    log_info(logger,"llegas esta acaaaaaaa");//+++++++++++++++++++++++++++++++++++++++++++++++++++
    int estado = tag_file(file_tag_origen,file_tag_destino);

    iniciar_mutex_file_tag(key_file_tag_destino); 

    pthread_mutex_unlock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 
    free(key_file_tag);
    free(key_file_tag_destino);
    free(file_tag_origen);
    free(file_tag_destino); 


    return estado; 
}
int atender_escritura(char* file, char* tag, int bloque, char* contenido,int tam_cont){
    char* key_file_tag = crear_key_file_tag(file,tag); 

    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);


    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary); 
        free(key_file_tag);
        return -1; 
    } 

    pthread_mutex_lock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 

    if (obtener_estado_file_tag(key_file_tag) == 0) { 
            log_error(logger, "ERROR WRITE: No se puede escribir en un tag COMMITED: %s", key_file_tag);
            pthread_mutex_unlock(mutex_file_tag);
            free(key_file_tag);
            return -1;
    }
    int cantidad_bloques = calcular_cant_bloq_log(file,tag);
    int estado_write;

   
    if(bloque >= cantidad_bloques){
        log_error(logger, "Error-WRITE: Se intentó WRITE en un bloque no existente de File:Tag : %s", key_file_tag);
        pthread_mutex_unlock(mutex_file_tag);
        free(key_file_tag);
        return -1;
    }
     
    estado_write = escritura_bloque(file, tag, bloque,contenido,tam_cont);
    if (estado_write > 0) {
        actualizar_metadata_bloque(file, tag, bloque, estado_write);
        estado_write = 0; 
    }

    

    pthread_mutex_unlock(mutex_file_tag);

    free(key_file_tag);

    return estado_write; 
}
int atender_lectura(char* file, char* tag, int bloque_logico, int* tamanio_leido, char** contenido_salida){
    char* key_file_tag = crear_key_file_tag(file,tag);  
    int estado_final; 
    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary);
        free(key_file_tag);
        *tamanio_leido = 0;
        *contenido_salida = NULL;
        return -1; 
    } 
    pthread_mutex_lock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 
 
    char* lectura = lectura_bloque(file,tag,bloque_logico, tamanio_leido);
    if (lectura == NULL) {
        log_error(logger, "Falló lectura_bloque para %s", key_file_tag);
        *contenido_salida = NULL;
        estado_final = -1;
    } else {
        *contenido_salida = lectura;
        estado_final = 0;
    }
    
    pthread_mutex_unlock(mutex_file_tag);
    free(key_file_tag);

    return estado_final;   
}
int atender_delete(char* file, char* tag){

    char* key_file_tag = crear_key_file_tag(file,tag);  
    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary);
        free(key_file_tag);
        return -1; 
    } 
    dictionary_remove(file_tag_dic, key_file_tag);
    pthread_mutex_unlock(&mutex_diccionary);

    actualizar_dicc_estado(key_file_tag, -1);//modificar

    pthread_mutex_lock(mutex_file_tag);
 
    int estado_borrado = eliminar_tag(file,tag);
    
    pthread_mutex_unlock(mutex_file_tag);
    pthread_mutex_destroy(mutex_file_tag);
    free(mutex_file_tag);
    free(key_file_tag);

    return estado_borrado;  
    
}
const char* operation_to_string(Operation op) {
    switch (op) {
        case CREATE:   return "CREATE";
        case TRUNCATE: return "TRUNCATE";
        case TAG:      return "TAG";
        case COMMIT:   return "COMMIT";
        case WRITE:    return "WRITE";
        case READ:     return "READ";
        case DELETE:   return "DELETE";
        default:       return "DESCONOCIDA";
    }
}
