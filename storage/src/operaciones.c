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
            return ERROR_FILE_TAG_PREEXISTENTE;
        } else {
            log_error(logger, "No se pudo crear el File %s. Error: %s", nombre_file, strerror(errno));
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
    fprintf(f, "BLOCKS=[]\n");

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
    return 0;
}

int truncar_archivo(char* file, char* tag, int nuevo_valor){
    char* ruta_file = add_seg_ruta(PUNTO_MONTAJE, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);          
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks"); 
    int tamanio_archivo = obtener_tamano(ruta_metadata);
    if(nuevo_valor < tamanio_archivo){
        incrementar(nuevo_valor, tamanio_archivo, ruta_L_blocks);
    }
    else {
        //decrementar(nuevo_valor, tamanio_archivo);
    }
    t_config* config = config_create(ruta_metadata);
    config_set_value(config, "TAMAÑO", tag);//mofidicar
    config_save(config);    
    return 0;           
} // falta desasignar 

void tag_file(char* origen, char* destino){
    copiar_directorio(origen, destino);
    char* ruta_metadata = add_seg_ruta(destino, "/metadata.config");
    t_config* config = config_create(ruta_metadata);
    config_set_value(config, "ESTADO", "WORK_IN_PROGRESS");
}


int commit_tag(char* file, char* tag){
    char* ruta_file = add_seg_ruta(PUNTO_MONTAJE, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    char* ruta_L_blocks = add_seg_ruta(ruta_file,"/logical_blocks");
    t_config* config = config_create(ruta_metadata);
    char* estado = config_get_string_value(config, "ESTADO");
    if(strcmp(estado,"COMMITED") == 0){
        recorrer_logical_blocks(ruta_L_blocks, ruta_tag);   
        config_set_value(config, "ESTADO", "COMMITED"); 
    
    }
    return 1;
}

int escritura_bloque(char* file, char* tag, int num_L_block, char* contenido){
    char* nombre_L_block = crear_nombre_block(num_L_block, 6);
    char* ruta_file = add_seg_ruta(PUNTO_MONTAJE, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    t_config* config = config_create(ruta_metadata);
    char* estado = config_get_string_value(config, "ESTADO");
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");
    char* ruta_L_block = add_seg_ruta(ruta_L_blocks, nombre_L_block);

    if(strcmp(estado,"COMMITED") == 0){ 
        struct stat st;
        if (stat(ruta_L_block, &st) == -1) {
            log_error(logger, "Bloque lógico no asignado o inexistente");
            return -1;
        }
        if (st.st_nlink == 1) { // verifica que solo hay un bloque logico asignado
            FILE* f = fopen(ruta_L_block, "r+b");
            if (!f) {
                log_error(logger, "fopen");
                return -1;
            }
            size_t tamanio = strlen(contenido);
            fwrite(contenido, 1, tamanio, f);
            fclose(f);
            printf("Escritura directa en el bloque físico.\n");
        } 
        else {
            // Buscar un bloque físico libre (supongamos que existe una función)


            int nuevo_bloque_fisico = buscar_primer_bloque_libre();
            char* nombre_nuevo_F_block = crear_nombre_block(nuevo_bloque_fisico, 4);
            char* ruta_F_blocks = add_seg_ruta(PUNTO_MONTAJE, "/physical_blocks");
            char* ruta_nuevo_F_block = add_seg_ruta(ruta_F_blocks, nombre_nuevo_F_block);
            
            FILE* f = fopen(ruta_L_block, "wb");
            if (!f) {
                log_error(logger, "fopen nuevo");
                return -1;
            }

            unlink(ruta_L_block);

            // escribo los datos en el bloque logico
            size_t tamanio = strlen(contenido);
            fwrite(contenido, 1, tamanio, f);

            fclose(f);

            // Actualizar el link lógico → apuntar al nuevo físico
            link(ruta_L_block, ruta_nuevo_F_block);

            printf("Se reasignó el bloque lógico a un nuevo bloque físico.\n");
    
        }
    }
    return 0;
}
 

char* lectura_bloque(char* file, char* tag, int num_L_block){
    char* nombre_L_block = crear_nombre_block(num_L_block, 6);
    char* ruta_file = add_seg_ruta(PUNTO_MONTAJE, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);         
    char* ruta_L_blocks = add_seg_ruta(ruta_tag,"/logical_blocks");
    char* ruta_L_block = add_seg_ruta(ruta_L_blocks, nombre_L_block);

    struct stat st;

    // se obtiene info del bloque lógico (y su bloque físico)
    if (stat(ruta_L_block, &st) == -1) {
        log_error(logger, "Error en stat");
        return NULL;
    }

    // se lee el contenido del bloque
    FILE* f = fopen(ruta_L_block, "rb");
    if (!f) {
        log_error(logger, "No se pudo abrir el bloque");
        return NULL;
    }

    char* buffer = malloc(st.st_size + 1); 
    if (!buffer) {
        log_error(logger, "No se pudo reservar memoria");
        fclose(f);
        return NULL;
    }

    size_t bytes_leidos = fread(buffer, 1, st.st_size, f);
    fclose(f);

    if (bytes_leidos != st.st_size) {
        log_error(logger, "Error al leer el bloque completo");
        free(buffer);
        return NULL;
    }
 
    return buffer;             
}

void eliminar_tag(char* tag){
    eliminar_directorio(tag);
    // FALTA DESASIGNAR BLOQUES
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

void incrementar(int nuevo_valor, int valor_original, char* ruta_logical_block){
    int cant_bloques = (nuevo_valor - valor_original) / BLOCK_SIZE;
    for(int i = 0; i < cant_bloques; i++){
        bloq_L_apuntan_bloq_F_0(ruta_logical_block);
    }
 }


//void decrementar(int nuevo_valor, int valor_original, char* ruta_tag){
//    int cant_bloques = (valor_original - nuevo_valor) / BLOCK_SIZE;
//    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
//    t_config* config = config_create(ruta_metadata);
//    char **bloques = config_get_array_value(config, "BLOCKS");
//    int cantidad = sizeof(bloques) / sizeof(bloques[0]);

//}

int bloq_L_apuntan_bloq_F_0(char* ruta_logical_block){
    int k = 4; 
    int bloque_fisico = 0;
    char* nombre_block = crear_nombre_block(bloque_fisico, k); 
    char* pre_ruta = add_seg_ruta("/physical_blocks",nombre_block);
    char* ruta_F_block = add_seg_ruta(PUNTO_MONTAJE,pre_ruta);

    int posicion = buscar_num_ultimo_bloque(ruta_logical_block);
    int Q = 6; 
    char* nombre_block_logic = crear_nombre_block(posicion, Q); 
    char* ruta_L_block_final= add_seg_ruta(ruta_logical_block, nombre_block_logic);
   
    if (link(ruta_F_block, ruta_L_block_final) == -1) {
        liberar_bloque_reservado(bloque_fisico);
        log_error(logger, "No se pudo crear Hard Link BASE. Error: %s", strerror(errno));


        free(nombre_block);
        free(pre_ruta);
        free(ruta_F_block); 
        free(nombre_block_logic);
        free(ruta_L_block_final);

        return -1; 

    }
    log_info(logger, "Hard link creado: %s -> %s", ruta_L_block_final, ruta_F_block);


    free(nombre_block);
    free(pre_ruta);
    free(ruta_F_block); 
    free(nombre_block_logic);
    free(ruta_L_block_final);

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


// funciones para commit_tag

void recorrer_logical_blocks(char* path_dir, char* ruta_tag) {
    DIR* dir = opendir(path_dir);  
    if (!dir) {
        log_error(logger, "No se pudo abrir el directorio");
        exit(EXIT_FAILURE);
    }

    struct dirent* entry;  
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // se construye la ruta completa del bloque lógico
        char ruta_bloque[512];
        snprintf(ruta_bloque, sizeof(ruta_bloque), "%s/%s", path_dir, entry->d_name);

        printf("Bloque lógico encontrado: %s\n", ruta_bloque);
        
        procesar_bloque_logico(ruta_bloque, i);
        eliminar_block_metadata(ruta_tag, i);
        i++;
    }

    closedir(dir);  
}


int procesar_bloque_logico(char* ruta_bloque, int contador) {
    struct stat st;

    // se obtiene info del bloque lógico (y su bloque físico)
    if (stat(ruta_bloque, &st) == -1) {
        log_error(logger, "Error en stat");
        return -1;
    }

    printf("Bloque lógico: %s\n", ruta_bloque);
    printf("Bloque físico (inodo): %ld\n", st.st_ino);
    printf("Tamaño del bloque: %ld bytes\n", st.st_size);
    printf("Cantidad de hard links: %ld\n", st.st_nlink);

    // se lee el contenido del bloque
    FILE* f = fopen(ruta_bloque, "rb");
    if (!f) {
        log_error(logger, "No se pudo abrir el bloque");
        return -1;
    }

    void* buffer = malloc(st.st_size);
    if (!buffer) {
        log_error(logger, "No se pudo reservar memoria");
        fclose(f);
        return -1;
    }

    fread(buffer, 1, st.st_size, f);
    fclose(f);

    // se calcula el hash del contenido
    char* hash = crypto_md5(buffer, st.st_size);
    free(buffer);

    if (!hash) {
        log_error(logger, "Error calculando hash MD5");
        return -1;
    }

    printf("Hash del bloque: %s\n", hash);
    
    if(config_has_property(config_hash, hash) == 0){
        // hacer que el bloque logico apunte al bloque fisico ya asignado
        int bloque_F = config_get_int_value(config_hash, hash);
        char* nombre_bloque_F = crear_nombre_block(bloque_F, 4);
        char* ruta_bloque_F = add_seg_ruta(PUNTO_MONTAJE, nombre_bloque_F);
        if (link(ruta_bloque_F, ruta_bloque) == -1) {

            liberar_bloque_reservado(bloque_F);
            log_error(logger, "No se pudo crear Hard Link BASE. Error: %s", strerror(errno));

            return -1; 

        }
        log_info(logger, "Hard link creado: %s -> %s", ruta_bloque, ruta_bloque_F);


    } else {
        // agregar al config hash
        int contador = 0;  
        char* bloque_fisico = crear_nombre_block(contador++, 4);
        config_set_value(config_hash, hash, bloque_fisico);
        config_save(config_hash);
        free(bloque_fisico);
    }

    free(hash);
    return 0;
}

void eliminar_block_metadata(char* ruta_tag, int posicion_bloq){
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    t_config* config = config_create(ruta_metadata);
    char **bloques = config_get_array_value(config, "BLOCKS");
    int pos = posicion_bloq; // posición a borrar

    for (int i = pos; bloques[i] != NULL; i++) {
        bloques[i] = bloques[i + 1];
    }

    // Reconstruir el string con formato [A,B,C,D]
    char nuevo_valor[512] = "[";
    for (int i = 0; bloques[i] != NULL; i++) {
        strcat(nuevo_valor, bloques[i]);
        if (bloques[i + 1] != NULL)
            strcat(nuevo_valor, ",");
    }
    strcat(nuevo_valor, "]");
    int valor = atoi(bloques[posicion_bloq]);
    liberar_bloque_reservado(valor);
    // Guardar en el config
    config_set_value(config, "BLOCKS", nuevo_valor);
    config_save(config);
    config_destroy(config);
}



