#include "operaciones.h"

#include "storage.h"        
#include "manejo-worker.h"



void create(char* nombre_file, char* nombre_tag, char* ruta) {
 
    char* nuevo_file = add_seg_ruta(ruta, nombre_file); 
    if (mkdir(nuevo_file, 0777) == -1) {
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
    //bitmap marca espacio ocupado
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