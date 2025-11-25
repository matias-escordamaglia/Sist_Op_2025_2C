#include "operaciones.h"

#include "storage.h"        
#include "manejo-worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <commons/crypto.h>

// Funciones nuevas
// CREATE TERMINADO
int create(char* nombre_file, char* nombre_tag) {
    int ERROR_FILE_TAG_PREEXISTENTE = -2;
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* nuevo_file = add_seg_ruta(ruta_files, nombre_file); 
    if (mkdir(nuevo_file, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "File %s ya existe", nombre_file);
             free(ruta_files);
            free(nuevo_file);
            return ERROR_FILE_TAG_PREEXISTENTE;
        } else {
            log_error(logger, "No se pudo crear el File %s. Error: %s", nombre_file, strerror(errno));
            free(ruta_files);
            free(nuevo_file);
            exit(EXIT_FAILURE);
        }
    }  
    char* nuevo_tag = add_seg_ruta(nuevo_file, nombre_tag); 
    if (mkdir(nuevo_tag, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "Tag %s ya existe", nombre_tag);
            return ERROR_FILE_TAG_PREEXISTENTE;
        } else {
            log_error(logger, "No se pudo crear el Tag %s. Error: %s", nombre_tag, strerror(errno));
            free(ruta_files);
            free(nuevo_file);
            free(nuevo_tag);
            exit(EXIT_FAILURE);
        }
    }  
    
    log_info(logger, "Creando metadata.config...");

    char* ruta_absoluta_metadata = add_seg_ruta(nuevo_tag,"/metadata.config"); 
    FILE* f = fopen(ruta_absoluta_metadata, "w");
        if (!f) {
            log_error(logger, "Error al crear metadata.config");
            free(ruta_files);
            free(nuevo_file);
            free(nuevo_tag);
            free(ruta_absoluta_metadata);
            exit(EXIT_FAILURE);
        }
    fprintf(f, "TAMAÑO=0\n");
    fprintf(f, "ESTADO=WORK_IN_PROGRESS\n");
    fprintf(f, "BLOCKS=[]\n");

    log_info(logger, "Metadata %s creado exitosamente",ruta_absoluta_metadata );
    fclose(f);

    char* ruta_absoluta_dir_log_block = add_seg_ruta(nuevo_tag,"/logical_blocks"); 
     if (mkdir(ruta_absoluta_dir_log_block, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "Directorio %s ya existe", ruta_absoluta_dir_log_block);
        } else {
            log_error(logger, "No se pudo crear el Directorio %s. Error: %s", ruta_absoluta_dir_log_block, strerror(errno));
            free(ruta_files);
            free(nuevo_file);
            free(nuevo_tag);
            free(ruta_absoluta_metadata);
            free(ruta_absoluta_dir_log_block);
            exit(EXIT_FAILURE);
        }
    }  
    else {
        log_info(logger, "Directorio %s creado correctamente", ruta_absoluta_dir_log_block); 
    }
    free(ruta_files);
    free(nuevo_file);
    free(nuevo_tag);
    free(ruta_absoluta_metadata);
    free(ruta_absoluta_dir_log_block);
    return 0;
}

int truncar_archivo(char* file, char* tag, int nuevo_valor){
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);          
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks"); 

    t_config* config_tag = config_create(ruta_metadata);
    if (config_tag == NULL) {
        log_error(logger, "TRUNCATE: No se pudo abrir metadata: %s", ruta_metadata);
        free(ruta_files);
        free(ruta_file);  
        free(ruta_tag);  
        free(ruta_metadata);  
        free(ruta_L_blocks);  

        return ERROR_DESCONOCIDO;
    }

    int tamanio_archivo = config_get_int_value(config_tag,"TAMAÑO");
    config_destroy(config_tag);

    int estado = 0; 
    if(nuevo_valor > tamanio_archivo){
        log_info(logger, "TRUNCATE: Incrementando FILE:TAG: %s:%s",file,tag);
        estado = incrementar(file,tag,nuevo_valor, tamanio_archivo, ruta_L_blocks);
    }
    else if (nuevo_valor < tamanio_archivo) {
        log_info(logger, "TRUNCATE: Decrementando FILE:TAG: %s:%s",file,tag);
        estado = decrementar(nuevo_valor, tamanio_archivo, ruta_tag);
    }
    if(estado==0){ 
        t_config* config_final = config_create(ruta_metadata);
        if (config_final) {
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%d", nuevo_valor);
            config_set_value(config_final, "TAMAÑO", buffer);
            config_save(config_final);
            config_destroy(config_final);
        } else {
            log_error(logger, "TRUNCATE: ¡Crítico! No se pudo reabrir config para setear TAMAÑO.");
            estado = -1;
        }
    }


    free(ruta_files);
    free(ruta_file);  
    free(ruta_tag);  
    free(ruta_metadata);  
    free(ruta_L_blocks);  

    return estado;           
} // falta desasignar 

int tag_file(char* origen, char* destino, char* file_origen, char* tag_origen,char* file_dest, char* tag_dest){
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_tag_origen = add_seg_ruta(ruta_files, origen);          
    char* ruta_tag_destino  = add_seg_ruta(ruta_files, destino);          
    
    if (mkdir(ruta_tag_destino, 0777) == -1) {
        log_error(logger, "TAG: Error creando directorio destino");
        free(ruta_files); free(ruta_tag_origen); free(ruta_tag_destino);
        
        return ERROR_DESCONOCIDO;
    }
    char* ruta_blocks_origen = add_seg_ruta(ruta_tag_origen, "/logical_blocks");
    char* ruta_blocks_destino = add_seg_ruta(ruta_tag_destino, "/logical_blocks");
    ;
    if (mkdir(ruta_blocks_destino, 0777) == -1) {
        log_error(logger, "TAG: Error creando directorio destino");
        borrar_directorio(ruta_tag_destino);
        free(ruta_files); free(ruta_tag_origen); free(ruta_tag_destino);
        free(ruta_blocks_destino); free(ruta_blocks_origen);
        return ERROR_DESCONOCIDO;
    }
    char* ruta_metadata_origen = add_seg_ruta(ruta_tag_origen, "/metadata.config");
    char* ruta_metadata_destino = add_seg_ruta(ruta_tag_destino, "/metadata.config");
    copiar_archivo(ruta_metadata_origen,ruta_metadata_destino);

    if (duplicar_enlaces_bloques(ruta_tag_origen, ruta_tag_destino,file_origen, tag_origen,file_dest,tag_dest)<0) {
        log_error(logger, "TAG: Error al duplicar enlaces");
        borrar_directorio(ruta_tag_destino);
        borrar_directorio(ruta_blocks_destino);
        free(ruta_files); free(ruta_tag_origen); free(ruta_tag_destino);
        free(ruta_blocks_destino); free(ruta_blocks_origen);
        free(ruta_metadata_destino); free(ruta_metadata_origen); 
        return ERROR_DESCONOCIDO; 
    };

    t_config* config_tag_destino = config_create(ruta_metadata_destino);
    if(config_tag_destino) {
        config_set_value(config_tag_destino, "ESTADO", "WORK_IN_PROGRESS");
        config_save(config_tag_destino);
        config_destroy(config_tag_destino);
    }

    free(ruta_files); free(ruta_tag_origen); free(ruta_tag_destino);
    free(ruta_blocks_destino); free(ruta_blocks_origen);
    free(ruta_metadata_destino); free(ruta_metadata_origen); 
    return 0; 
}
int duplicar_enlaces_bloques(char* ruta_tag_origen, char* ruta_tag_destino,char* file_origen, char* tag_origen,char* file_dest, char* tag_dest){
    char* ruta_physical_blocks = add_seg_ruta(PUNTO_MONTAJE, "/physical_blocks");
    char* ruta_metadata_origen = add_seg_ruta(ruta_tag_origen, "/metadata.config");
    char* ruta_logical_blocks_destino= add_seg_ruta(ruta_tag_destino,"/logical_blocks");
    t_config* config_metadata_origen = config_create(ruta_metadata_origen);
    char** blocks_array = config_get_array_value(config_metadata_origen, "BLOCKS");

    int i = 0;

    while(blocks_array[i] != NULL) {
        int nro_fisico_actual = atoi(blocks_array[i]);
        
        char* nombre_L_block_dest = crear_nombre_block(i, 6);
        char* ruta_L_block_destino = add_seg_ruta(ruta_logical_blocks_destino, nombre_L_block_dest);

        char* nombre_bloque_fisico_origen =  crear_nombre_block(nro_fisico_actual,4); 
        char* ruta_F_block_origen = add_seg_ruta(ruta_physical_blocks,nombre_bloque_fisico_origen);

        if (link(ruta_F_block_origen, ruta_L_block_destino) == -1) { //LOG LISTO
            log_error(logger, "No se pudo crear Hard Link para %s. Error: %s", nombre_L_block_dest, strerror(errno));
            free(nombre_L_block_dest); free(nombre_bloque_fisico_origen); 
            free(ruta_L_block_destino); free(ruta_F_block_origen);
            return ERROR_DESCONOCIDO; 
        }
        int duplic_bloque_f= obtener_nro_bloque_fisico(file_origen,tag_origen,i); 
        log_info(logger,"##%u - %s:%s  Se agregó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,file_dest,tag_dest,i,duplic_bloque_f);

        
        
        free(nombre_L_block_dest); free(nombre_bloque_fisico_origen); 
        free(ruta_L_block_destino); free(ruta_F_block_origen);

        i++;
    }
    string_array_destroy(blocks_array);  
    config_destroy(config_metadata_origen);  
    
    free(ruta_physical_blocks);
    free(ruta_metadata_origen);
    free(ruta_logical_blocks_destino);
    return 0; 
}


int commit_tag(char* file, char* tag){
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE,"/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    char* ruta_logical_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");

    t_config* config_tag = config_create(ruta_metadata);

    if (!config_tag) {
        log_error(logger, "COMMIT: No se puede crear el config");
        config_destroy(config_tag);
        free(ruta_files);free(ruta_file);free(ruta_tag);  
        free(ruta_metadata);free(ruta_logical_blocks); 
        return ERROR_DESCONOCIDO;
    }

    char** blocks_array = config_get_array_value(config_tag, "BLOCKS");
    int i = 0;
    bool hubo_cambios = false; 
    char* key_file_tag = crear_key_file_tag(file,tag); 
    while(blocks_array[i] != NULL) {
        int nro_fisico_actual = atoi(blocks_array[i]);
        
        char* nombre_L_block = crear_nombre_block(i, 6);
        char* ruta_L_block = add_seg_ruta(ruta_logical_blocks, nombre_L_block);

        int nro_fisico_final = procesar_bloque_logico(ruta_L_block, nro_fisico_actual,file,tag,i);
        log_info(logger,"##%u - %s Se agregó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,key_file_tag,i,nro_fisico_final); 


        if (nro_fisico_final < 0) {
            log_error(logger, "Error en commit del bloque %d", i);
            // ... manejo de error grave ...
        } 
        else if (nro_fisico_final != nro_fisico_actual) {
            // ¡Hubo deduplicación! Actualizamos el array en memoria
            free(blocks_array[i]);
            blocks_array[i] = string_itoa(nro_fisico_final);
            hubo_cambios = true;
        }

        free(nombre_L_block);
        free(ruta_L_block);
        i++;
    }
    free(key_file_tag); 
    // 4. Si hubo cambios (deduplicación), guardamos la nueva lista de bloques
        if (hubo_cambios) {
            char* joined_blocks = join_string_array(blocks_array, ",");
            char* formatted_blocks = string_from_format("[%s]", joined_blocks);
            config_set_value(config_tag, "BLOCKS", formatted_blocks);
            free(joined_blocks);
            free(formatted_blocks);
        }
        // 5. Marcar como COMMITED
        config_set_value(config_tag, "ESTADO", "COMMITED");
        config_save(config_tag);

        // Limpieza
        string_array_destroy(blocks_array);
        config_destroy(config_tag);
        free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata); free(ruta_logical_blocks);
        
        return 0;
}
int procesar_bloque_logico(char* ruta_bloque, int nro_bloque_fisico_actual, char* file , char* tag, int bloque_logico) {
    struct stat st;
    if (stat(ruta_bloque, &st) == -1) {
        log_error(logger, "Error en stat: %s", ruta_bloque);
        return -1;
    }

    FILE* f = fopen(ruta_bloque, "rb");
        if (!f) 
            return ERROR_DESCONOCIDO;

    void* buffer = malloc(st.st_size);
    fread(buffer, 1, st.st_size, f);
    fclose(f);

    char* hash = crypto_md5(buffer, st.st_size);
    free(buffer);

    log_info(logger, "Procesando bloque Físico %d - Hash: %s", nro_bloque_fisico_actual, hash);

    int bloque_fisico_final = nro_bloque_fisico_actual; // Por defecto, nos quedamos con el mismo

    pthread_mutex_lock(&mutex_file_hash);

    if (config_has_property(config_hash, hash)) {
        
        int nro_bloque_existente = config_get_int_value(config_hash, hash); 
        
        
        log_info(logger, "--> Hash encontrado en bloque %d. Deduplicando...", nro_bloque_existente);

        if (nro_bloque_existente != nro_bloque_fisico_actual) {
            char* nombre_bloque_F = crear_nombre_block(nro_bloque_existente, 4);
            char* ruta_files = add_seg_ruta(PUNTO_MONTAJE,"/physical_blocks");
            char* ruta_bloque_F = add_seg_ruta(ruta_files, nombre_bloque_F);

            unlink(ruta_bloque); //LOG LISTO
            log_info(logger,"##%u - %s:%s Se eliminó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,file,tag,bloque_logico,nro_bloque_fisico_actual);

            
            if (link(ruta_bloque_F, ruta_bloque) == -1) {// log en otra funcion madre
                log_error(logger, "Error al relinkear");
                // Manejo de error...
            } else {
                liberar_bloque_si_no_se_usa(nro_bloque_fisico_actual);
                //log_info(logger,"##%u - %s Se agregó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,)
                bloque_fisico_final = nro_bloque_existente;
            }
            free(nombre_bloque_F); free(ruta_bloque_F);free(ruta_files);
        }

    } else {
        log_info(logger, "--> Hash nuevo. Indexando bloque %d.", nro_bloque_fisico_actual);
        char* str_nro_bloque = string_itoa(nro_bloque_fisico_actual);
        config_set_value(config_hash, hash, str_nro_bloque);
        config_save(config_hash);
        free(str_nro_bloque);
        
    }

    pthread_mutex_unlock(&mutex_file_hash);
    free(hash);

    return bloque_fisico_final;
}

int escritura_bloque(char* file, char* tag, int num_L_block, char* contenido,int tamanio){

    if(tamanio > BLOCK_SIZE){ 
            log_error(logger, "ERROR WRITE: desbordamiento de bloque físico (Tamaño: %u)", tamanio);
        return ERROR_FUERA_DE_LIMITE;
    }
    char* nombre_L_block = crear_nombre_block(num_L_block, 6);
    char* key_file_tag = crear_key_file_tag(file,tag); 
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files,file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");
    char* ruta_L_block = add_seg_ruta(ruta_L_blocks, nombre_L_block);

    struct stat st;
    if (stat(ruta_L_block, &st) == -1) {
        log_error(logger, "Bloque lógico no asignado o inexistente");

        free(nombre_L_block); free(key_file_tag); free(ruta_files); free(ruta_file); 
        free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
        return ERROR_DESCONOCIDO;
    }
    if (st.st_nlink == 2) { // verifica que solo hay un bloque logico asignado

        log_info(logger, "WRITE: Escritura directa (nlink==2)");
        //log_contenido_legible(logger, "Contenido WRITE recibido", contenido, tamanio);
        usleep(RETARDO_ACCESO_BLOQUE * 1000);

        FILE* f = fopen(ruta_L_block, "r+b");
        if (!f) {
            log_error(logger, "ERROR WRITE: no de puedo abrir el archivo: %s", ruta_L_block);
            free(nombre_L_block); free(key_file_tag); free(ruta_files); free(ruta_file); 
            free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block); 
            return ERROR_DESCONOCIDO;
        }
        
        fwrite(contenido, 1, tamanio, f);

        fclose(f);

        free(nombre_L_block); free(key_file_tag); free(ruta_files); free(ruta_file); 
        free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block); 
        return 0; 
    }
    else {
        log_info(logger, "WRITE: Bloque compartido (nlink==%d). Aplicando COW.", (int)st.st_nlink);
        int k=4; 
        int bloque_fisico = encontrar_y_reservar_bloque(); 
        if (bloque_fisico == -1) {
            log_error(logger, "Espacio insuficiente en el bitmap");

            free(nombre_L_block); free(key_file_tag); free(ruta_files); free(ruta_file); 
            free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block); 
            return ERROR_ESPACIO_INSUFICIENTE;
        }
        char* nombre_block = crear_nombre_block(bloque_fisico, k); 
        char* pre_ruta = add_seg_ruta("/physical_blocks",nombre_block);
        char* ruta_F_block = add_seg_ruta(PUNTO_MONTAJE,pre_ruta);
            
        
        usleep(RETARDO_ACCESO_BLOQUE * 1000);

        FILE* f = fopen(ruta_F_block, "wb");
        if (!f) {
            log_error(logger, "ERROR al abrir el archivo %s",pre_ruta);
            liberar_bloque_reservado(bloque_fisico); 
            free(nombre_L_block); free(key_file_tag); free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
            free(nombre_block); free(pre_ruta); free(ruta_F_block);
            return ERROR_DESCONOCIDO;
        }
        // escribo los datos en el bloque logico
        fwrite(contenido, 1, tamanio, f);
        fclose(f);

        unlink(ruta_L_block); //LOG LISTO
        int bloque_fisico_viejo = obtener_nro_bloque_fisico(file,tag,num_L_block);

        log_info(logger,"##%u - %s Se eliminó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,key_file_tag,num_L_block,bloque_fisico_viejo);



        if (link(ruta_F_block, ruta_L_block) == -1) { //LOG LISTO

            liberar_bloque_reservado(bloque_fisico);
            log_error(logger, "No se pudo crear Hard Link. Error: %s", strerror(errno));
            free(nombre_L_block); free(key_file_tag); free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
            free(nombre_block); free(pre_ruta); free(ruta_F_block);
            return ERROR_DESCONOCIDO; 
        }

        log_info(logger,"##%u - %s Se agregó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,key_file_tag,num_L_block,bloque_fisico);

        log_info(logger,"WRITE: COW finalizado. Bloque lógico %d ahora apunta a físico %d", num_L_block, bloque_fisico);

        free(nombre_L_block); free(key_file_tag); free(ruta_files);
        free(ruta_file); free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
        free(nombre_block); free(pre_ruta); free(ruta_F_block);
        return bloque_fisico;
    }
}
 

char* lectura_bloque(char* file, char* tag, int num_L_block, int* tamanio_leido ){
    char* nombre_L_block = crear_nombre_block(num_L_block, 6);
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);         
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");
    char* ruta_L_block = add_seg_ruta(ruta_L_blocks, nombre_L_block);

    *tamanio_leido = 0;

    struct stat st;

    // se obtiene info del bloque lógico (y su bloque físico)
    if (stat(ruta_L_block, &st) == -1) {
        log_error(logger, "Error en stat de %s: %s", ruta_L_block, strerror(errno));
        free(nombre_L_block); free(ruta_files); free(ruta_file); 
        free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
        return NULL;
    }
    usleep(RETARDO_ACCESO_BLOQUE * 1000);
    // se lee el contenido del bloque
    FILE* f = fopen(ruta_L_block, "rb");
    if (!f) {
        log_error(logger, "No se pudo abrir el bloque: %s", ruta_L_block);
        free(nombre_L_block); free(ruta_files); free(ruta_file); 
        free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
        return NULL;
    }

    char* buffer = malloc(st.st_size); 
    if (!buffer) {
        log_error(logger, "No se pudo reservar memoria para el bloque");
        fclose(f);
        free(nombre_L_block); free(ruta_files); free(ruta_file); 
        free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
        return NULL;
    }

    size_t bytes_leidos = fread(buffer, 1, st.st_size, f);
    fclose(f);

    if (bytes_leidos != st.st_size) {
        log_error(logger, "Error al leer el bloque completo (leídos %zu de %ld)", bytes_leidos, st.st_size);
        free(buffer); 
        free(nombre_L_block); free(ruta_files); free(ruta_file); 
        free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
        return NULL;
    }
    free(nombre_L_block); free(ruta_files); free(ruta_file); 
    free(ruta_tag); free(ruta_L_blocks); free(ruta_L_block);
    
    
    *tamanio_leido = (int)st.st_size;
 
    return buffer;             
}

int eliminar_tag(char* file, char* tag){
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag); 
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");  
    char **bloques = config_get_array_value(config, "BLOCKS");
    t_config* config = config_create(ruta_metadata);

    if (config == NULL) {
        log_error(logger, "DELETE: No se pudo leer metadata de %s", ruta_metadata);
        free(ruta_files);free(ruta_file);
        free(ruta_tag);free(ruta_metadata);        
        return ERROR_DESCONOCIDO;
    }
    int cantidad_bloques = 0;
    while (bloques[cantidad_bloques] != NULL) {
        cantidad_bloques++;
    }
    for(int i = 0; i < cantidad_bloques; i++){
        char* nombre_L_block = crear_nombre_block(i, 6);
        char* ruta_L_block = add_seg_ruta(nombre_L_block, ruta_L_blocks);
        struct stat st;
        if (st.st_nlink == 2) { 
            if (stat(ruta_L_block, &st) == -1) {
                log_error(logger, "Bloque lógico no asignado o inexistente");
                return ERROR_DESCONOCIDO;
            }
            liberar_bloque_reservado(i);
        }

    }
    eliminar_directorio(ruta_tag);
    free(ruta_files), free(ruta_file), free(ruta_tag), free(ruta_metadata), free(ruta_L_blocks);
    return 0;
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

// funciones para truncate

int incrementar(char*file,char*tag, int nuevo_valor, int valor_original, char* ruta_logical_block){
    int bloques_actuales = (int)ceil((double)valor_original / (double)BLOCK_SIZE);
    int bloques_necesarios = (int)ceil((double)nuevo_valor / (double)BLOCK_SIZE);
    int cant_bloques_a_agregar = bloques_necesarios - bloques_actuales;

    if (cant_bloques_a_agregar <= 0) {
        log_warning(logger, "TRUNCATE: Incremento no resultó en bloques nuevos.");
        return 0; 
    }

    int* bloques_fisicos_nuevos = malloc(cant_bloques_a_agregar * sizeof(int));
    if (!bloques_fisicos_nuevos) {
        log_error(logger, "TRUNCATE: Falló malloc para el array de bloques");
        return ERROR_DESCONOCIDO;
    }

    int nuevo_bloque_f;

    for(int i = 0; i < cant_bloques_a_agregar; i++){

        int bloque_logico_a_crear = bloques_actuales + i;

        nuevo_bloque_f = asignar_bloque_logico_especifico(ruta_logical_block,bloque_logico_a_crear);
        log_info(logger,"##%u - %s:%s  Se agregó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,file,tag,i,nuevo_bloque_f);


        if(nuevo_bloque_f<0){
            rollback_falla_incrementar(bloques_fisicos_nuevos,i); 
            free(bloques_fisicos_nuevos);
            return ERROR_ESPACIO_INSUFICIENTE; 
        }

        bloques_fisicos_nuevos[i]=nuevo_bloque_f;
    }
    
    int estado_meta = actualizar_metadata_incremento(file, tag, bloques_fisicos_nuevos, cant_bloques_a_agregar);
    
    free(bloques_fisicos_nuevos);
    return estado_meta; 
}


int decrementar(int nuevo_valor, int valor_original, char* ruta_tag){
    int cant_eliminar_bloques = (valor_original - nuevo_valor) / BLOCK_SIZE;
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    t_config* config = config_create(ruta_metadata);
    char **bloques = config_get_array_value(config, "BLOCKS");
    
    
    int cantidad_bloques = 0;
    while (bloques[cantidad_bloques] != NULL) {
        cantidad_bloques++;
    }
    
    for (int pos = cantidad_bloques - 1; pos >= cantidad_bloques - cant_eliminar_bloques; pos--) {
        char* nombre_bloque = crear_nombre_block(pos, 6);
        char* ruta_L_block = add_seg_ruta(ruta_L_blocks, nombre_bloque);
        
        struct stat st;
        if (stat(ruta_L_block, &st) == -1) {
            log_error(logger, "Bloque lógico no asignado o inexistente");
            return ERROR_DESCONOCIDO;
        }
        if (st.st_nlink == 1) { // verifica que solo hay un bloque logico asignado
            FILE* f = fopen(ruta_L_block, "rb");
            if (!f) {
                log_error(logger, "No se pudo abrir el bloque");
                return ERROR_DESCONOCIDO;
            }

            void* buffer = malloc(st.st_size);
            if (!buffer) {
                log_error(logger, "No se pudo reservar memoria");
                fclose(f);
                return ERROR_DESCONOCIDO;
            }

            fread(buffer, 1, st.st_size, f);
            fclose(f);

            // se calcula el hash del contenido
            char* hash = crypto_md5(buffer, st.st_size);
            free(buffer);

            if (!hash) {
                log_error(logger, "Error calculando hash MD5");
                return ERROR_DESCONOCIDO;
            }
            int bloque_F = config_get_int_value(config_hash, hash);
            liberar_bloque_reservado(bloque_F);
        }
        
        // Desasociar el bloque lógico
        unlink(ruta_L_block); //LOG PENDIENTE 
        //log_info(logger,"##%u - %s:%s Se eliminó el hard link del bloque lógico %u al bloque físico %u",g_query_id_actual,file,tag,bloque_logico,nro_bloque_fisico_actual);

    }
    return 0;
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

int copiar_directorio(char* dir_origen, char* dir_destino) {
    mkdir(dir_destino, 0777);  
    
    DIR* dir = opendir(dir_origen);     
    if (!dir) {
        log_error(logger, "No se pudo abrir el directorio origen");
        return ERROR_DESCONOCIDO;
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
    return 0; 

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

    while((entrada = readdir(dir)) != NULL) {
        // ignorar "." y ".."
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

    if (rmdir(directorio) != 0)
        log_error(logger, "Error eliminando directorio");
    else
        printf("Directorio '%s' eliminado correctamente.\n", directorio);
}


