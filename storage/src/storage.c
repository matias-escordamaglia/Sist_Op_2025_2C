#include "storage.h"

t_log *logger;
t_config *config;
t_config *sp_block_config;

int server_fd_general;

pthread_t hilo_manejo_worker;

int main(int argc, char **argv)
{

    //config = iniciar_config(logger, "storage.config");
    config = config_create("storage.config"); 
    extraer_storage_config(config);

    log_level = obtener_log_level_config(config);
    logger = log_create("storage.log", "STORAGE", true, log_level);

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

void extraer_storage_config(t_config* config_st)
{
    PUERTO_ESCUCHA = config_get_string_value(config_st, "PUERTO_ESCUCHA");
    PUNTO_MONTAJE = config_get_string_value(config_st, "PUNTO_MONTAJE");
    RETARDO_OPERACION = config_get_int_value(config_st, "RETARDO_OPERACION");
    RETARDO_ACCESO_BLOQUE = config_get_int_value(config_st, "RETARDO_ACCESO_BLOQUE");
    char *fresh_star = config_get_string_value(config_st, "FRESH_START");
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
            if (remove(ruta_block_hash) == 0) {
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
        inicializar_blocks_hash(ruta_block_hash);
        inicializar_dir_physic_block(ruta_f_block); 
        inicializar_bitmap(ruta_bitmap);
        inicializar_dir_logic_block(ruta_files);
        log_info(logger, "TODAS LAS ESTRUCTURAS ESTA LISTAS");
 

    }
    else
    {
        //cargar estructuras existentes
        //cargar_bitmap();    
    }
}
void finalizar_FS(){
    config_destroy(config);
    log_destroy(logger); 
    bitarray_destroy ( BA_bitmap);

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
    sp_block_config = config_create("superblock.config");
    BLOCK_SIZE = config_get_int_value(sp_block_config, "BLOCK_SIZE");
    FS_SIZE = config_get_int_value(sp_block_config, "FS_SIZE");
    log_info(logger, "Archivo superblock.config extraido exitosamente");
    pasar_log_config_a_manejo_worker(logger,sp_block_config);


}
void inicializar_bitmap(const char* ruta){
    //ya que vamos a usar mmap menor utilizamos file descriptors 
    int tam_bitmap = ((FS_SIZE / BLOCK_SIZE + 7)/8) ; 
    int fd = open( ruta, O_RDWR | O_CREAT, 0666); 
    if (fd == -1) {
        log_error(logger, "error abriendo %s: %s", ruta, strerror(errno));
        exit(1);
    }
    if (ftruncate(fd, tam_bitmap) == -1) {
        log_error(logger, "error truncado %s: %s", ruta, strerror(errno)); 
        close(fd);
        exit(1);
    }
    log_info(logger, "Archivo bitmap.bin creado exitosamente"); 
    
    log_info(logger, "mapeando bitmap.bin..."); 
    char* mmap_BM = mmap(NULL,tam_bitmap,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
        if (mmap_BM == MAP_FAILED) {
        log_error(logger, "error mapeo %s: %s", ruta, strerror(errno)); 
             exit(1); 
        }
    memset(mmap_BM, 0, tam_bitmap);
    BA_bitmap = bitarray_create_with_mode(mmap_BM,tam_bitmap,MSB_FIRST); 
    log_info(logger, "BITMAP Creado exitosamente"); 
    close(fd); 

}
void inicializar_dir_physic_block( char* ruta){
    ///home/utnso/tp-2025-2c-SegFaulteadores-Seriales/storage
    //log_info(logger, "ruta fs; %s", "/home/utnso/storage/physical_blocks");
    if (mkdir(ruta, 0777) == -1) {
        log_info(logger, "ERROR: Directorio %s no creado", ruta); 
        if (errno == EEXIST) {
            log_info(logger, "Directorio %s ya existe", ruta);
        } else {
            log_error(logger, "No se pudo crear el directorio %s. Error: %s", ruta, strerror(errno));
            free(ruta);
            exit(EXIT_FAILURE);
        }
        }
    
    else {
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
        //log_info(logger,"Bloque %s  fué creado exitosamente",ruta_block);
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
void inicializar_dir_logic_block( char* ruta){

    if (mkdir(ruta, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "Directorio %s ya existe", ruta);
        } else {
            log_error(logger, "No se pudo crear el Directorio %s. Error: %s", ruta, strerror(errno));
            free(ruta);
            exit(EXIT_FAILURE);
        }
    }  
    else {
        log_info(logger, "Directorio %s creado correctamente", ruta); 
    }


    char* ruta_initial_file = add_seg_ruta(ruta,"/initial_file"); 
    if (mkdir(ruta_initial_file, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "File %s ya existe", ruta_initial_file);
        } else {
            log_error(logger, "No se pudo crear el File %s. Error: %s", ruta_initial_file, strerror(errno));
            free(ruta_initial_file);
            exit(EXIT_FAILURE);
        }
    }  
    else {
        log_info(logger, "File %s creado correctamente", ruta_initial_file); 
    }

     char* ruta_tag_BASE = add_seg_ruta(ruta_initial_file    ,"/BASE"); 
    if (mkdir(ruta_tag_BASE, 0777) == -1) {
        if (errno == EEXIST) {
            log_info(logger, "Tag %s ya existe", ruta_tag_BASE);
        } else {
            log_error(logger, "No se pudo crear el Tag %s. Error: %s", ruta_tag_BASE, strerror(errno));
            free(ruta_tag_BASE);
            exit(EXIT_FAILURE);
        }
    }  
    else {
        log_info(logger, "Tag %s creado correctamente", ruta_tag_BASE); 
    }


//metadata
    log_info(logger, "Creando metadata.config...");

    char* ruta_absoluta_metadata = add_seg_ruta(ruta_tag_BASE,"/metadata.config"); 
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


//logical block-> hard_links
    char* ruta_absoluta_dir_log_block = add_seg_ruta(ruta_tag_BASE,"/logical_blocks"); 
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
// dentro de esta carpeta creamos el hard link del B. Físico 0.
    char* ruta_F_block_Base = add_seg_ruta(PUNTO_MONTAJE, "/physical_blocks/block0000.dat");
    char* ruta_L_block_Base = add_seg_ruta(ruta_absoluta_dir_log_block, "/000000.dat");
//creación de hard link 
    if (link(ruta_F_block_Base, ruta_L_block_Base) == -1) {
    log_error(logger, "No se pudo crear Hard Link BASE. Error: %s", strerror(errno));
    exit(EXIT_FAILURE);
    }
    log_info(logger, "Hard link BASE creado");



    free(ruta_initial_file);
    free(ruta_tag_BASE);
    free(ruta_absoluta_metadata);
    free(ruta_absoluta_dir_log_block);
    free(ruta_F_block_Base);
    free(ruta_L_block_Base);

}

void crear_metadata_config(char* ruta){
    char* ruta_absoluta_metadata = add_seg_ruta(ruta,"/metadata.config"); 
    FILE* f = fopen(ruta_absoluta_metadata, "w");
         if (!f) {
        perror("Error al crear metadata.config");
        exit(EXIT_FAILURE);
    }
}
void inicializar_blocks_hash(char* ruta){
    //aca solo creamos el archivo y lo seteamos
    FILE* f = fopen(ruta,"wb");
    if (!f) { perror("Error creando blocks_hash_index"); exit(1); }
    //rellenamos con valores seteados
    int cant_bloques = (FS_SIZE/BLOCK_SIZE); 
    for(int i=0;i<cant_bloques;i++){
        fprintf(f, "=%d\n", i);  // vacío = bloque libre
    }
    log_info(logger,"Block_hash.config creado correctamente");
    //para la manipulacion de datos usaremos t_config* 
    config_hash = config_create(ruta);
    fclose(f);

}
int busqueda_block_asociado_hash(char* hash){
    if(config_has_property(config,hash)){ //checkea si existe el hash
        int bloque = config_get_int_value(config,hash); //devuelve el n° de bloque
        return bloque; 
    }
    return -1; 
}

