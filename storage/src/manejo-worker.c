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
int cantidad_workers;
pthread_mutex_t mutex_cant_workers; 

void pasar_log_config_a_manejo_worker(t_log* l, t_config* c) {
    blockconfig = c; 
    logger_worker = l;
} 
void* manejar_cliente_worker(void* arg) {
    int server_fd = (*(int*)arg);
    free(arg);
    log_info(logger,"Esperando conexiones..."); 

    pthread_mutex_init(&mutex_cant_workers,NULL); 

    pthread_mutex_lock(&mutex_cant_workers);
    cantidad_workers = 0 ; 
    pthread_mutex_unlock(&mutex_cant_workers);

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
    
    pthread_mutex_lock(&mutex_cant_workers);
    cantidad_workers++; 
    log_info(logger_worker, "##Se conecta el Worker %u - Cantidad de Workers: %u", id_worker, cantidad_workers); 

    pthread_mutex_unlock(&mutex_cant_workers); 

    
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
                    g_query_id_actual = -1; 
                    int query_id = extraer_int(buffer_st, &offset);
                    g_query_id_actual = query_id; 
                    char* nombre_file = extraer_string(buffer_st,&offset); 
                    char* nombre_tag  = extraer_string(buffer_st,&offset);

                    //log_info(logger_worker, "Aplicando RETARDO_OPERACION para OP: %d", operation);
                    usleep(RETARDO_OPERACION * 100);

                    int estado = -1; 
                    char* contenido_salida;
                    int tamanio_leido;
                    
                log_info(logger_worker, "##%u EJECUTANDO OPERACIÓN: %s", query_id, operation_to_string(operation));

                    switch (operation){
                        case  CREATE:
                            estado = atender_create(nombre_file,nombre_tag,query_id);

                            break;
                        case  TRUNCATE:
                            int tamanio = (int)extraer_uint32(buffer_st,&offset);
                            estado = atender_truncate(nombre_file,nombre_tag,tamanio,query_id); 
                            break;      
                        case TAG: 
                            char* file_destino = extraer_string(buffer_st,&offset); 
                            char* tag_destino  = extraer_string(buffer_st,&offset);
                            estado = atender_tag(nombre_file,nombre_tag,file_destino,tag_destino,query_id); 
                            free(file_destino);
                            free(tag_destino);
                            break;
                        case COMMIT:
                            estado = atender_commit(nombre_file,nombre_tag,query_id);
                            break;
                        case WRITE:
                            int bloque = (int)extraer_int(buffer_st,&offset);
                            int tamanio_contenido ; 
                            char* contenido = extraer_binario_y_tamanio(buffer_st, &offset,&tamanio_contenido);
                            log_contenido_legible(logger_worker, "Contenido WRITE recibido", contenido, tamanio_contenido); 
                            estado = atender_escritura(nombre_file, nombre_tag,bloque,contenido,tamanio_contenido,query_id);
                            free(contenido); 
                            break;
                        case READ: 
                            int bloque_logico = extraer_int(buffer_st,&offset);
                            tamanio_leido=0; 
                            contenido_salida = NULL;
                            estado = atender_lectura(nombre_file,nombre_tag,bloque_logico,&tamanio_leido,&contenido_salida,query_id); 
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
    enviar_paquete(paquete,socket);
}

int atender_create(char* file, char* tag, int query_id){
    char* key_file_tag = crear_key_file_tag(file,tag);  
    int estado; 
    pthread_mutex_lock(&mutex_diccionary);

    if (dictionary_has_key(file_tag_dic, key_file_tag)){
        log_error(logger, "Error: Se intentó operar sobre un File:Tag Existente: %s", key_file_tag);
        free(key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary); 
        return ERROR_FILE_TAG_PREEXISTENTE; 
    }else {
        estado = create(file,tag);
        if(estado==0){
            iniciar_mutex_file_tag(key_file_tag);
            anadir_a_dicc_estado(key_file_tag); 
            log_info(logger,"##%u - File Creado %s", query_id, key_file_tag);

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
int atender_truncate(char* file, char* tag,int tamanio, int query_id){
    char* key_file_tag = crear_key_file_tag(file,tag); 

    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);


    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary);
        free(key_file_tag);
        return ERROR_FILE_TAG_INEXISTENTE; 
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
        if(estado_truncate==0)
            log_info(logger,"##%u - File Truncado %s - Tamaño: %u",query_id, key_file_tag,tamanio); 
    }

    pthread_mutex_unlock(mutex_file_tag);

    free(key_file_tag);

    return estado_truncate; 
}

int atender_tag(char* file, char* tag, char* file_destino,char* tag_destino, int query_id){
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
        return ERROR_FILE_TAG_PREEXISTENTE; 
    }
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        free(key_file_tag);
        free(key_file_tag_destino);
        free(file_tag_origen);
        free(file_tag_destino); 
        pthread_mutex_unlock(&mutex_diccionary); 
        return ERROR_FILE_TAG_INEXISTENTE; 
    } 
    pthread_mutex_lock(mutex_file_tag);
    int estado = tag_file(file_tag_origen,file_tag_destino,file,tag,file_destino, tag_destino);

    if(estado==0){
        log_info(logger,"##%u - Tag creado %s",query_id,key_file_tag_destino); 
        iniciar_mutex_file_tag(key_file_tag_destino); 
        anadir_a_dicc_estado(key_file_tag_destino); 

    }


    pthread_mutex_unlock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 
    free(key_file_tag);
    free(key_file_tag_destino);
    free(file_tag_origen);
    free(file_tag_destino); 


    return estado; 
}
int atender_commit(char* file, char* tag, int query_id){
    char* key_file_tag = crear_key_file_tag(file,tag);  

    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary);
        free(key_file_tag);
        return ERROR_FILE_TAG_INEXISTENTE; 
    } 
    pthread_mutex_lock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 

    if (obtener_estado_file_tag(key_file_tag) == 0) {//cambiar esto para cualquier negativo en caso de error
        log_warning(logger, "Warning: Se intentó COMMIT sobre un tag ya commiteado: %s", key_file_tag);
        pthread_mutex_unlock(mutex_file_tag);
        free(key_file_tag);
        return ERROR_NO_CRITICO; ///no es error pero no se puedo commitear 
    }
    int estado = commit_tag(file,tag);

    int estado_dic = 1; ;
    if(estado==0){
        estado_dic = actualizar_dicc_estado(key_file_tag,0); 
    }
    pthread_mutex_unlock(mutex_file_tag);


    if(estado==0 && estado_dic == 0){ 
        log_info(logger,"##%u - Commit de File:Tag %s", query_id,key_file_tag);
        free(key_file_tag);
        return 0; 
    }
    free(key_file_tag);
   return ERROR_DESCONOCIDO; 
}
int atender_lectura(char* file, char* tag, int bloque_logico, int* tamanio_leido, char** contenido_salida, int query_id){
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
        return ERROR_FILE_TAG_INEXISTENTE; 
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
        log_contenido_legible(logger, "Contenido READ leido", *contenido_salida, *tamanio_leido);
        log_info(logger, "##%u - Bloque Lógico Leído %s - Número de Bloque: %u",query_id,key_file_tag,bloque_logico); 
    }
    
    pthread_mutex_unlock(mutex_file_tag);
    free(key_file_tag);

    return estado_final;   
}
int atender_escritura(char* file, char* tag, int bloque, char* contenido,int tam_cont, int query_id){
    char* key_file_tag = crear_key_file_tag(file,tag); 

    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);


    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary); 
        free(key_file_tag);
        return ERROR_FILE_TAG_INEXISTENTE;
    } 

    pthread_mutex_lock(mutex_file_tag);
    pthread_mutex_unlock(&mutex_diccionary); 

    if (obtener_estado_file_tag(key_file_tag) == 0) { 
            log_error(logger, "ERROR WRITE: No se puede escribir en un tag COMMITED: %s", key_file_tag);
            pthread_mutex_unlock(mutex_file_tag);
            free(key_file_tag);
            return ERROR_ESCRITURA_NO_PERMITIDA;
    }
    int cantidad_bloques = calcular_cant_bloq_log(file,tag);
    int estado_write;

   
    if(bloque >= cantidad_bloques){
        log_error(logger, "Error-WRITE: Se intentó WRITE en un bloque no existente de File:Tag : %s", key_file_tag);
        pthread_mutex_unlock(mutex_file_tag);
        free(key_file_tag);
        return ERROR_FILE_TAG_INEXISTENTE;
    }
     
    estado_write = escritura_bloque(file, tag, bloque,contenido,tam_cont); //estado_write tambien puede ser el nuevo bloque fisico
    if (estado_write > 0) {
       int estado_actualizacion = actualizar_metadata_bloque(file, tag, bloque, estado_write);
        if(estado_actualizacion==0){

        estado_write = 0; 
        log_info(logger,"##%u - Bloque Lógico Escrito %s - Número de Bloque: %u",query_id,key_file_tag,bloque);

        }else
            //roll_back
           estado_write= estado_actualizacion;
        

        };

    

    pthread_mutex_unlock(mutex_file_tag);

    free(key_file_tag);

    return estado_write; 
}
int atender_delete(char* file, char* tag, int query_id){

    char* key_file_tag = crear_key_file_tag(file,tag);  
    pthread_mutex_lock(&mutex_diccionary); 

    pthread_mutex_t* mutex_file_tag = dictionary_get(file_tag_dic,key_file_tag);
    if (mutex_file_tag == NULL) {
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_diccionary);
        free(key_file_tag);
        return ERROR_FILE_TAG_INEXISTENTE; 
    } 
    dictionary_remove(file_tag_dic, key_file_tag);
    pthread_mutex_unlock(&mutex_diccionary);

    actualizar_dicc_estado(key_file_tag, 0);//modificar

    pthread_mutex_lock(mutex_file_tag);
 
    int estado_borrado = eliminar_tag(file,tag);
    if(estado_borrado==0){
        log_info(logger, "##%u- Tag Eliminado %s", query_id,key_file_tag); 
    }
    
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
