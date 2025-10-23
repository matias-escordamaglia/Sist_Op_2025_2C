#include "manejo-worker.h"

#include "operaciones.h"  
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

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

// Funciones nuevas

void create(char* nombre_file, char* nombre_tag, char* ruta) {
 
    char* nuevo_file = add_seg_ruta(ruta, nombre_file); 
    if (mkdir(PUNTO_MONTAJE, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "File %s ya existe", nombre_file);
        } else {
            log_error(logger, "No se pudo crear el File %s. Error: %s", nombre_file, strerror(errno));
            free(nuevo_file);
            exit(EXIT_FAILURE);
        }
    }  
    char* nuevo_tag = add_seg_ruta(nuevo_file, nombre_tag); 
    if (mkdir(nuevo_tag, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "File %s ya existe", nombre_tag);
        } else {
            log_error(logger, "No se pudo crear el File %s. Error: %s", nombre_tag, strerror(errno));
            free(nuevo_tag);
            exit(EXIT_FAILURE);
        }
    }  
    
    log_info(logger, "Creando metadata.config...");

    char* ruta_absoluta_metadata = add_seg_ruta(nuevo_tag,"/metadata.config"); 
    FILE* f = fopen(ruta_absoluta_metadata, "w");
        if (!f) {
            log_error(logger, "Error al crear metadata.config");
            exit(EXIT_FAILURE);
        }
    fprintf(f, "TAMAÑO=0\n");
    fprintf(f, "ESTADO=WORK_IN_PROGRESS\n");
    fprintf(f, "BLOCKS=[0]\n");
  
    bitarray_set_bit(BA_bitmap,0); 

    log_info(logger, "Metadata %s creado exitosamente",ruta_absoluta_metadata );
    fclose(f);

    char* ruta_absoluta_dir_log_block = add_seg_ruta(nuevo_tag,"/logical_blocks"); 
     if (mkdir(ruta_absoluta_dir_log_block, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "Directorio %s ya existe", ruta_absoluta_dir_log_block);
        } else {
            log_error(logger, "No se pudo crear el Directorio %s. Error: %s", ruta_absoluta_dir_log_block, strerror(errno));
            free(ruta_absoluta_dir_log_block);
            exit(EXIT_FAILURE);
        }
    }  
    else {
        log_info(logger, "Directorio %s creado correctamente", ruta_absoluta_dir_log_block); 
    }

    free(nuevo_file);
    free(nuevo_tag);
    free(ruta_absoluta_metadata);
    free(ruta_absoluta_dir_log_block);
}

void truncar_archivo(char* file, char* tag, char* nuevo_valor){
    char* ruta_file = add_seg_ruta(PUNTO_MONTAJE, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);          
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config"); 
    int tamanio_archivo = obtener_tamano(ruta_metadata);
    if(nuevo_valor < tamanio_archivo){
        incrementar(nuevo_valor, tamanio_archivo);
    }
    else {
        decrementar(nuevo_valor, tamanio_archivo);
    }
    config_set_value(ruta_metadata, "TAMAÑO", nuevo_valor);
    config_save(ruta_metadata);               
} // falta desasignar y asignar bloques

void tag_file(char* origen, char* destino){
    copiar_diretorio(origen, destino);
    char* ruta_metadata = add_seg_ruta(origen, "/metadata.config");
    config_set_value(ruta_metadata, "ESTADO", "WORK_IN_PROGRESS");
}


void commit_tag(){

}

void escritura_bloque(){

}

void lectura_bloque(){

}

void eliminar_tag(char* tag){
    eliminar_directorio(tag);
}

int obtener_tamano(char* ruta) {
    FILE* f = fopen(ruta, "r");  
    if (!f) {
        log_error(logger, "No se pudo abrir metadata.config");
        exit(EXIT_FAILURE);
    }

    char linea[128];
    int tamano = -1;

    while (fgets(linea, sizeof(linea), f)) {  
        if (strncmp(linea, "TAMAÑO=", 7) == 0) {  
            tamano = atoi(linea + 7);  
            break; 
        }
    }
    fclose(f);
    return tamano;
}

void incrementar(int nuevo_valor, int valor_original){
    int cant_bloques = (nuevo_valor - valor_original) / BLOCK_SIZE;
}


void decrementar(int nuevo_valor, int valor_original){
    int cant_bloques = (valor_original - nuevo_valor) / BLOCK_SIZE;
}




// Funciones para tag_file

void copiar_archivo(char* archivo_origen, char* archivo_destino) {
    FILE* src = fopen(archivo_origen, "rb");   
    FILE* dst = fopen(archivo_destino, "wb");  
    if (!src || !dst) {                
        log_error(logger, "Error abriendo archivos");
        if (src) fclose(src);
        if (dst) fclose(dst);
        return;
    }

    char buffer[4096];                 
    size_t bytes;
        
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst); 
    }

    fclose(src);
    fclose(dst);
}

void copiar_directorio(char* dir_origen, char* dir_destino) {
    mkdir(dir_destino, 0777);  
    
    DIR* dir = opendir(dir_origen);     
    if (!dir) {
        log_error(logger, "No se pudo abrir el directorio origen");
        return;
    }

    struct dirent *entrada;         
    char ruta_origen[1024]; 
    char ruta_destino[1024];

    while ((entrada = readdir(dir)) != NULL) {  
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0)
            continue;                            

        snprintf(ruta_origen, sizeof(ruta_origen), "%s/%s", dir_origen, entrada->d_name);
        snprintf(ruta_destino, sizeof(ruta_destino), "%s/%s", dir_destino, entrada->d_name);

        struct stat info;
        stat(ruta_origen, &info);              

        if (S_ISDIR(info.st_mode)) {
            copiar_directorio(ruta_origen, ruta_destino); 
        } else {
            copiar_archivo(ruta_origen, ruta_destino);    
        }
    }

    closedir(dir);
}

// funciones para eliminar_tag

void eliminar_directorio(char* directorio) {
    DIR *dir = opendir(directorio);
    if (!dir) {
        log_error(logger, "No se pudo abrir el directorio");
        return;
    }

    struct dirent *entrada;
    char path[1024];

    while ((entrada = readdir(dir)) != NULL) {
        // Ignorar "." y ".."
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", directorio, entrada->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                eliminar_directorio(path);
            } else {
                if (remove(path) != 0)
                    log_error(logger, "Error eliminando archivo");
            }
        }
    }

    closedir(dir);

    // Finalmente, eliminar el directorio vacío
    if (rmdir(directorio) != 0)
        log_error(logger, "Error eliminando directorio");
    else
        printf("Directorio '%s' eliminado correctamente.\n", ruta);
}