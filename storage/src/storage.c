#include "storage.h"

t_log *logger;
t_config *config;
t_config *sp_block_config;

int server_fd_general;

pthread_t hilo_manejo_worker;

int main(int argc, char **argv)
{

    config = iniciar_config(logger, "storage.config");
    extraer_storage_config(config);

    log_level = obtener_log_level_config(config);
    logger = log_create("storage.log", "STORAGE", true, log_level);
    pasar_logger_a_manejo_worker(logger);

    iniciar_estructuras();

    server_fd_general = iniciar_servidor(NULL, PUERTO_ESCUCHA, logger);
    if (server_fd_general == -1)
    {
        log_error(logger, "No se pudo iniciar el servidor general. Terminando.");
        return EXIT_FAILURE;
    }

    int *server_fd_copy = malloc(sizeof(int));
    *server_fd_copy = server_fd_general;
    pthread_create(&hilo_manejo_worker, NULL, manejar_cliente_worker, server_fd_copy);
    pthread_join(hilo_manejo_worker, NULL);

    close(server_fd_general);
    log_info(logger, "Servidor general cerrado correctamente.");

    return EXIT_SUCCESS;
}

void extraer_storage_config(t_config *config)
{
    PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");
    PUNTO_MONTAJE = config_get_string_value(config, "PUNTO_MONTAJE");
    RETARDO_OPERACION = config_get_int_value(config, "RETARDO_OPERACION");
    RETARDO_ACCESO_BLOQUE = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
    char *fresh_star = config_get_string_value(config, "FRESH_START");
    if (strcmp(fresh_star, "TRUE") == 0)
    {
        FRESH_START = true;
    }
    else
    {
        FRESH_START = false;
    }
}
void iniciar_estructuras(){
    

    if (FRESH_START == true){ // Iniciamos un FS desde cero
        log_info(logger, "Iniciando seteo de File System... ");
        char* ruta_bitmap = add_seg_ruta(PUNTO_MONTAJE, "/bitmap.bin"); 
        char* ruta_block_hash = add_seg_ruta(PUNTO_MONTAJE, "/blocks_hash_index.config"); 
        char* ruta_f_block = add_seg_ruta(PUNTO_MONTAJE, "/physical_blocks");
        char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");

        if(existe_archivo(ruta_bitmap)){
            if (remove(ruta_bitmap) == 0) {
                log_info(logger, "Archivo %s borrado correctamente\n", ruta_bitmap);
            } else {
                log_error(logger, "Error al borrar el archivo");
            }       
        }
        if(existe_archivo(ruta_block_hash)){
            if (remove(ruta_bitmap) == 0) {
                log_info(logger, "Archivo %s borrado correctamente\n", ruta_block_hash);
            } else {
                log_error(logger, "Error al borrar el archivo");
            }       
        }
        if(existe_directorio(ruta_f_block)==1){
            int estado = borrar_directorio(ruta_f_block); 
            if(estado==0){
                log_info(logger, "Directorio %s borrado correctamente", ruta_f_block); 
            }
            else {
                log_error(logger, "Error al borrar el directorio");
            }
        }
         if(existe_directorio(ruta_files)==1){
            int estado = borrar_directorio(ruta_files); 
            if(estado == 0){
                log_info(logger, "Directorio %s borrado correctamente", ruta_files); 
            }
            else {
                log_error(logger, "Error al borrar el directorio");
            }
        }
        log_info(logger, "Inicializando estructuras nuevas...");
        inicializar_super_block_config();
        inicializar_dir_phys_block(ruta_f_block){
        inicializar_bitmap(ruta_bitmap);



    }
    else
    {

        //cargar estructuras existentes
        //cargar_bitmap();
        return 0; 
    
    }
}
bool existe_archivo(char *path){
    FILE *f = fopen(path, "r");
    if (f){
        fclose(f);
        return true;
    }
    return false; 
}
char* add_seg_ruta(char *base, char *extra){
    size_t len_base = strlen(base);
    size_t len_extra = strlen(extra);
    char *ruta_final = malloc(len_base + len_extra + 1);
    if (!ruta_final)
    {
        perror("Error al realloc");
        exit(EXIT_FAILURE);
    }
    strcpy(ruta_final, base);
    strcpy(ruta_final + len_base, extra);

    return ruta_final;
}
int existe_directorio(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}
int borrar_directorio(const char *path) {
    struct stat st;

    // 1. Verificar si existe y es directorio
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            return 0; // no existe, nada que borrar
        }
        perror("stat");
        return -1; // otro error
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s no es un directorio\n", path);
        return -1;
    }

    // 2. Abrir el directorio
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    char fullpath[PATH_MAX];

    // 3. Iterar sobre las entradas
    while ((entry = readdir(dir)) != NULL) {
        // ignorar "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // recursivo: borrar subdirectorio
                if (borrar_directorio(fullpath) != 0) {
                    closedir(dir);
                    return -1;
                }
            } else {
                // borrar archivo
                if (remove(fullpath) != 0) {
                    perror("remove archivo");
                    closedir(dir);
                    return -1;
                }
            }
        }
    }

    closedir(dir);

    // 4. Borrar el directorio vacío
    if (rmdir(path) != 0) {
        perror("rmdir");
        return -1;
    }

    return 0; // éxito
}
void inicializar_super_block_config(){
    char *ruta_sp_block_config = add_seg_ruta(PUNTO_MONTAJE, "/superblock.config");
    sp_block_config = config_create(ruta_sp_block_config);
    BLOCK_SIZE = config_get_int_value(sp_block_config, "BLOCK_SIZE");
    FS_SIZE = config_get_int_value(sp_block_config, "FS_SIZE");
}
void inicializar_bitmap(const char* ruta){
    FILE* f = fopen("bitmap.bin", "w+r"); 


}
void inicializar_dir_phys_block( char* ruta){
    if (mkdir(ruta, 0777) == -1) {
        log_info(logger, "ERROR: Directorio %s no creado", ruta); 
    } else {
        log_info(logger, "Directorio %s creado correctamente", ruta); 
    }
    int cant_bloques = (FS_SIZE / BLOCK_SIZE); 

    log_info(logger, "Iniciando creacion de bloques físicos...");
    log_info(logger, "Tamanio FL: %d",FS_SIZE);
    log_info(logger, "Tamanio bloque: %d",BLOCK_SIZE);
    log_info(logger, "Cantidad de bloques: %d",cant_bloques);
    
    char ruta_block[50]; 

    for(int i=0; i<cant_bloques;i++){
        snprintf(ruta_block, sizeof(ruta_block), "/block%04d.dat", i);
        char* ruta_final = add_seg_ruta(ruta, ruta_block);
        crear_bloque(ruta_final, BLOCK_SIZE); 
        log_info(logger,"Bloque %s  fué creado exitosamente",ruta_block);
        free(ruta_final); 
        if(i == cant_bloques - 1)
            log_info(logger, "Se crearon %d bloques exitosamente", cant_bloques);
    }
}

void crear_bloque(const char* ruta, size_t block_size) {
    FILE *f = fopen(ruta, "wb");
    if (!f) { perror("fopen"); exit(1); }
    // Rellenar con ceros para asegurar tamaño fijo
    char *buffer = calloc(1, block_size);
    fwrite(buffer, 1, block_size, f);
    free(buffer);
    fclose(f);
}