#include "storage.h"

#include "manejo-worker.h"
#include "operaciones.h"

t_log *logger;
t_config *config;
t_config *sp_block_config;

//get configs
char* PUERTO_ESCUCHA; 
bool FRESH_START;
char* PUNTO_MONTAJE;
int RETARDO_OPERACION;
int RETARDO_ACCESO_BLOQUE; 

//hash config
t_config* config_hash; 

int BLOCK_SIZE; 
int FS_SIZE; 

// bitarray
t_bitarray* BA_bitmap; 
char* mmap_BM;


t_log_level log_level; 

int server_fd_general;

pthread_t hilo_manejo_worker;

//semaforos mutex
pthread_mutex_t mutex_bitmap;
pthread_mutex_t mutex_dir_files; 
pthread_mutex_t mutex_file_hash;
pthread_mutex_t mutex_diccionary; 
pthread_mutex_t mutex_dic_estado; 


//dictionarys
t_dictionary* file_tag_dic = NULL; 
t_dictionary* dicc_estado_tag = NULL; 

__thread int g_query_id_actual = -1;


int main(int argc, char **argv)
{   
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sighandler);

    if (argc < 3) { 
            fprintf(stderr, "Uso correcto: %s <archivo_config[path]> <archivo_superBlock[path]> \n", argv[0]);
            return EXIT_FAILURE;
    }
    
    char* archivo_superBlock_path = argv[2];
    char* archivo_config_path = argv[1];

    t_log* log_temp = log_create("temp.log","STORAGE",true,LOG_LEVEL_INFO); 
    config = iniciar_config(log_temp,archivo_config_path);
    extraer_storage_config(config);
    log_destroy(log_temp); 

    log_level = obtener_log_level_config(config);
    logger = log_create("storage.log", "STORAGE", true, log_level);

    iniciar_estructuras(archivo_superBlock_path);


    log_info(logger, "Preparando Servidor...");

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
void iniciar_estructuras(char* super_block_path){
    

    if (FRESH_START == true){ // Iniciamos un FS desde cero
        log_info(logger, "Iniciando seteo de File System... ");

        if (mkdir(PUNTO_MONTAJE, 0777) == -1) {
            if (errno == EEXIST) {
                log_info(logger, "Directorio punto de montaje %s ya existe", PUNTO_MONTAJE);
            } else {
                log_error(logger, "No se pudo crear el directorio punto de montaje %s. Error: %s", 
                         PUNTO_MONTAJE, strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else {
            log_info(logger, "Directorio punto de montaje %s creado correctamente", PUNTO_MONTAJE);
        }

        char* ruta_bitmap = add_seg_ruta(PUNTO_MONTAJE, "/bitmap.bin"); 
        char* ruta_block_hash = add_seg_ruta(PUNTO_MONTAJE, "/blocks_hash_index.config"); 
        char* ruta_f_block = add_seg_ruta(PUNTO_MONTAJE, "/physical_blocks");
        char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    

        if(existe_archivo(ruta_bitmap)){
            if (remove(ruta_bitmap) == 0) {
                log_info(logger, "Archivo %s borrado correctamente\n", ruta_bitmap);
            } else {
                log_error(logger, "Error al borrar el archivo");
                exit(EXIT_FAILURE);
            }       
        }
        if(existe_archivo(ruta_block_hash)){
            if (remove(ruta_block_hash) == 0) {
                log_info(logger, "Archivo %s borrado correctamente\n", ruta_block_hash);
            } else {
                log_error(logger, "Error al borrar el archivo");
            exit(EXIT_FAILURE);

            }       
        }
        if(existe_directorio(ruta_f_block)==1){
            int estado = borrar_directorio(ruta_f_block); 
            if(estado==0){
                log_info(logger, "Directorio %s borrado correctamente", ruta_f_block); 
            }
            else {
                log_error(logger, "Error al borrar el directorio");
                exit(EXIT_FAILURE);
            
            }
        }
         if(existe_directorio(ruta_files)==1){
            int estado = borrar_directorio(ruta_files); 
            if(estado == 0){
                log_info(logger, "Directorio %s borrado correctamente", ruta_files); 
            }
            else {
                log_error(logger, "Error al borrar el directorio");
                exit(EXIT_FAILURE);

            }
        }
        log_info(logger, "Inicializando estructuras nuevas...");
        inicializar_super_block_config(super_block_path);
        inicializar_dictionary_mutex();
        inicializar_blocks_hash(ruta_block_hash);
        inicializar_bitmap(ruta_bitmap);
        inicializar_dir_physic_block(ruta_f_block); 
        inicializar_dir_logic_block(ruta_files);
        
        free(ruta_bitmap);
        free(ruta_block_hash);
        free(ruta_f_block);
        free(ruta_files);

    }
    else
    {
        cargar_estructuras_existentes(super_block_path);
         
    }
    log_info(logger, "TODAS LAS ESTRUCTURAS ESTA LISTAS");
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
    int necesita_barra = (extra[0] != '/');
    size_t len_base = strlen(base);
    size_t len_extra = strlen(extra);
    size_t total = len_base + len_extra + (necesita_barra ? 1 : 0) + 1;

    char* ruta_final = malloc(total);
    if (!ruta_final) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(ruta_final, base);
    if (necesita_barra)
        strcat(ruta_final, "/");
    strcat(ruta_final, extra);

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
void inicializar_super_block_config(char* path){
    sp_block_config = iniciar_config(logger,path);
    BLOCK_SIZE = config_get_int_value(sp_block_config, "BLOCK_SIZE");
    FS_SIZE = config_get_int_value(sp_block_config, "FS_SIZE");
    log_info(logger, "Archivo superblock.config extraido exitosamente");
    pasar_log_config_a_manejo_worker(logger,sp_block_config);


}
void inicializar_bitmap(const char* ruta){
    //ya que vamos a usar mmap menor utilizamos file descriptors 
    //aca creamos un archivo .bin
    //tam_bitmap = malloc(sizeof(int)); 
    int tam_bitmap = ((FS_SIZE / BLOCK_SIZE + 7)/8) ; 
    log_info(logger, "tamaño: %u", tam_bitmap); 

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
    //"subimos" el archivo a memoria 
    log_info(logger, "mapeando bitmap.bin..."); 
    mmap_BM = mmap(NULL,tam_bitmap,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 
        if (mmap_BM == MAP_FAILED) {
        log_error(logger, "error mapeo %s: %s", ruta, strerror(errno)); 
             exit(1); 
        }
    //lo llenamos de ceros
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
    char* key_initial = crear_key_file_tag("initial_file","BASE"); 
    anadir_a_dicc_estado(key_initial);
    iniciar_mutex_file_tag(key_initial);


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

    FILE* F = fopen(ruta_F_block_Base, "wb");
    if (!F) {
        log_error(logger, "No se pudo abrir el bloque INICIAL: %s", ruta_F_block_Base);
        free(ruta_initial_file);
        exit(EXIT_FAILURE);
    }

    char* buffer = malloc(BLOCK_SIZE); 
    if (!buffer) {
        log_error(logger, "No se pudo reservar memoria para el bloque");
        fclose(F);
        exit(EXIT_FAILURE);
    }
    memset(buffer, '0', BLOCK_SIZE);
    fwrite(buffer,1,BLOCK_SIZE,f);
    free(buffer); 
    fclose(F); 
    log_info(logger, "Bloque Físico 0 relleno de ceros para Initial File");

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
void inicializar_semaforos(){
    pthread_mutex_init(&mutex_bitmap,NULL);
    pthread_mutex_init(&mutex_dir_files,NULL);
    pthread_mutex_init(&mutex_file_hash,NULL);
    pthread_mutex_init(&mutex_diccionary,NULL);
}
//seccion critica 
void iniciar_mutex_file_tag(char* nombre){
    pthread_mutex_t* nuevo_mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(nuevo_mutex, NULL);
    dictionary_put(file_tag_dic, nombre, nuevo_mutex);
}
//seccion critica 
void eliminar_mutex_file_tag(char* nombre){
    if(dictionary_has_key(file_tag_dic,nombre)==true){
        pthread_mutex_t* mutex_a_eliminar = (pthread_mutex_t*) dictionary_remove(file_tag_dic, nombre);
        pthread_mutex_destroy(mutex_a_eliminar);
    } 
}
void cargar_estructuras_existentes(char* super_block_path){
    log_info(logger, "Iniciando en modo FRESH_STAR = false");

    char* ruta_bitmap = add_seg_ruta(PUNTO_MONTAJE, "/bitmap.bin"); 
    char* ruta_block_hash = add_seg_ruta(PUNTO_MONTAJE, "/blocks_hash_index.config"); 
    char* ruta_f_block = add_seg_ruta(PUNTO_MONTAJE, "/physical_blocks");
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");

    log_info(logger, "Cargando estructuras existentes");
    inicializar_dictionary_mutex();
    inicializar_super_block_config(super_block_path);
    cargar_block_hash(ruta_block_hash); 
    cargar_bitmap(ruta_bitmap);
    mapeo_dir_mutex_dinamic(ruta_files); 
     
    free(ruta_bitmap);
    free(ruta_block_hash);
    free(ruta_f_block);
    free(ruta_files);
}
void cargar_block_hash(char* ruta){
    config_hash = config_create(ruta); 
}
void cargar_bitmap(char* ruta){
    log_info(logger, "mapeando bitmap.bin..."); 
    int tam_bitmap = ((FS_SIZE / BLOCK_SIZE + 7)/8) ; 
    log_info(logger, "tamaño: %u", tam_bitmap);
    int fd = open(ruta,O_RDWR);
    if (fd == -1) {
        log_error(logger, "Error abriendo %s: %s", ruta, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (ftruncate(fd, tam_bitmap) == -1) {
        log_error(logger, "Error con ftruncate en %s: %s", ruta, strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "tamaño: %u", tam_bitmap); 
    char* mmap_BM = mmap(NULL,tam_bitmap,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_BM == MAP_FAILED) {
    log_error(logger, "error mapeo %s: %s", ruta, strerror(errno)); 
            exit(1); 
    }
    BA_bitmap = bitarray_create_with_mode(mmap_BM,tam_bitmap,MSB_FIRST); 
      if (!BA_bitmap) {
        log_error(logger, "Error creando bitarray desde el mmap de %s", ruta);
        munmap(mmap_BM, tam_bitmap);
        close(fd);
        exit(EXIT_FAILURE);
    }
    log_info(logger, "BITMAP Cargado exitosamente"); 
    close(fd);
}

void mapeo_dir_mutex_dinamic(char* ruta){
    DIR* dir_files = opendir(ruta);
        if (dir_files == NULL) {
            log_error(logger, "Error al abrir el directorio");
            return;
        }

        struct dirent* entrada_file;

        while ((entrada_file = readdir(dir_files)) != NULL) {
            
            if (strcmp(entrada_file->d_name, ".") == 0 || strcmp(entrada_file->d_name, "..") == 0) {
                continue;
            }

            if (entrada_file->d_type == DT_DIR) {
                char* nombre_file = entrada_file->d_name;
                char* ruta_file = add_seg_ruta(ruta,nombre_file); 
                
                DIR* dir_tags = opendir(ruta_file);
                if (dir_tags == NULL) {
                    log_error(logger, "Error al abrir el directorio");
                    continue;
                }

                struct dirent* entrada_tag;
                while ((entrada_tag = readdir(dir_tags)) != NULL) {
                    if (strcmp(entrada_tag->d_name, ".") == 0 || strcmp(entrada_tag->d_name, "..") == 0) {
                        continue;
                    }

                    if (entrada_tag->d_type == DT_DIR) {
                        char* nombre_tag = entrada_tag->d_name;
                        char* ruta_tag = add_seg_ruta(ruta_file,nombre_tag);
                        log_info(logger, "File:Tag descubierto: %s:%s", nombre_file, nombre_tag);
               
                     // recreando diccionario de mutex y diccionario de estado

                        char* key_file_tag = crear_key_file_tag(nombre_file, nombre_tag); 
                        log_info(logger, "valor: %s", key_file_tag);

                        int estado_leido = lectura_metadata(ruta_tag);
                        log_info(logger, "estado: %u", estado_leido);

                        if(estado_leido == 1 || estado_leido == 0  ){
                            intptr_t estado_ptr = estado_leido; 
                            //log_info(logger, "DEBUG: dicc_estado_tag=%p, key=%s, estado_ptr=%p", (void*)dicc_estado_tag, key_file_tag, (void*)estado_ptr);
                            dictionary_put(dicc_estado_tag, key_file_tag, (void*)(intptr_t)estado_ptr);
                            log_info(logger, "File:Tag añadido a diccionario de ESTASDo: %s:%s", nombre_file, nombre_tag);
                        }else
                            log_error(logger, "Error de lectura metadata: %s", key_file_tag); 
                    
                     //crear y agregamos al dicc el mutex file_tag
                        iniciar_mutex_file_tag(key_file_tag); 
                        log_info(logger, "File:Tag añadido a diccionario de MUTEX: %s:%s", nombre_file, nombre_tag);


                     free(ruta_tag);
                     free(key_file_tag);
                      
                    }
                }
                log_info(logger, "Mapeo de Files Finalizado");
                free(ruta_file);
                closedir(dir_tags);
            }
        }
        closedir(dir_files);
}
char* crear_key_file_tag(char *nombre_file,  char *nombre_tag){
    size_t len_file = strlen(nombre_file);
    size_t len_tag = strlen(nombre_tag);
    
    char *key = malloc(len_file + len_tag + 2);
    if (!key){
        log_info(logger, "error de malloc");
       exit(EXIT_FAILURE);
    }

    sprintf(key, "%s:%s", nombre_file, nombre_tag);
    return key; 
}
int lectura_metadata(char* ruta){
    char* ruta_metadata = add_seg_ruta(ruta, "/metadata.config");
    //log_info(logger, "ruta metadata: %s",ruta_metadata); 
    int estado = -1;  

    t_config* config_temporal = config_create(ruta_metadata);
    if (config_temporal == NULL) {
        log_error(logger, "Error al abrir  Metadata");
        free(ruta_metadata); 

        return estado; 
    }
    
    if (!config_has_property(config_temporal, "ESTADO")) {
        log_error(logger, "La clave 'ESTADO' no existe en: %s", ruta_metadata);
        config_destroy(config_temporal);
        free(ruta_metadata);
        return estado;
    }

    char* estado_C = config_get_string_value(config_temporal, "ESTADO");
    if((strcmp(estado_C,"COMMITED")==0)){

        estado = 0; //COMMITED
    }
    else 
        estado = 1; //WORK_IN_PROGRESS

    config_destroy(config_temporal);
    free(ruta_metadata); 
    return estado;
}


void inicializar_dictionary_mutex(){
    if (dicc_estado_tag == NULL)
    dicc_estado_tag = dictionary_create();
    if (file_tag_dic == NULL)
        file_tag_dic = dictionary_create();
}
// --- Esta función debe ser llamada DENTRO de un mutex del File:Tag ---
int asignar_bloque_logico(char* ruta_logical_block){
// buscamos un bloque fisico
// dentro de esta carpeta creamos el hard link del B. Físico 0. 
    int k=4; 
    int bloque_fisico = encontrar_y_reservar_bloque(); 
    if (bloque_fisico == -1) {
        log_error(logger, "Espacio insuficiente en el bitmap");
        return ERROR_ESPACIO_INSUFICIENTE;
    }
    char* nombre_block = crear_nombre_block(bloque_fisico, k); 
    char* pre_ruta = add_seg_ruta("/physical_blocks",nombre_block);
    char* ruta_F_block = add_seg_ruta(PUNTO_MONTAJE,pre_ruta);
//encontrar numero de bloque logico a esta ruta
    int posicion = buscar_num_ultimo_bloque(ruta_logical_block);
    if(posicion<0){
        free(nombre_block);
        free(pre_ruta);
        free(ruta_F_block);
        liberar_bloque_reservado(bloque_fisico); 
        return ERROR_DESCONOCIDO; 
    }
    int Q = 6; 
    char* nombre_block_logic = crear_nombre_block(posicion, Q); 
    char* ruta_L_block_final= add_seg_ruta(ruta_logical_block, nombre_block_logic);
//creación de hard link 
    if (link(ruta_F_block, ruta_L_block_final) == -1) {

        liberar_bloque_reservado(bloque_fisico);
        log_error(logger, "No se pudo crear Hard Link BASE. Error: %s", strerror(errno));


        free(nombre_block);
        free(pre_ruta);
        free(ruta_F_block); 
        free(nombre_block_logic);
        free(ruta_L_block_final);

        return ERROR_DESCONOCIDO; 

    }
    log_info(logger, "Hard link creado: %s -> %s", ruta_L_block_final, ruta_F_block);


    free(nombre_block);
    free(pre_ruta);
    free(ruta_F_block); 
    free(nombre_block_logic);
    free(ruta_L_block_final);

    return bloque_fisico; 
}
int asignar_bloque_logico_especifico(char* ruta_logical_block, int num_bloque_logico) {
    
    int k = 4; 
    int bloque_fisico = encontrar_y_reservar_bloque(); 
    log_info(logger,"##%u - Bloque Físico Reservado - Número de Bloque: %u",g_query_id_actual, bloque_fisico);
    if (bloque_fisico == -1) {
        log_error(logger, "Espacio insuficiente en el bitmap");
        return ERROR_ESPACIO_INSUFICIENTE;
    }
    
    char* nombre_block = crear_nombre_block(bloque_fisico, k); 
    char* pre_ruta = add_seg_ruta("/physical_blocks", nombre_block);
    char* ruta_F_block = add_seg_ruta(PUNTO_MONTAJE, pre_ruta);

    int Q = 6; 
    char* nombre_block_logic = crear_nombre_block(num_bloque_logico, Q); 
    char* ruta_L_block_final = add_seg_ruta(ruta_logical_block, nombre_block_logic);

    if (link(ruta_F_block, ruta_L_block_final) == -1) {
        liberar_bloque_reservado(bloque_fisico); 
        log_error(logger, "No se pudo crear Hard Link para %s. Error: %s", nombre_block_logic, strerror(errno));
        free(nombre_block); free(pre_ruta); free(ruta_F_block); 
        free(nombre_block_logic); free(ruta_L_block_final);
        return ERROR_DESCONOCIDO; 
    }
    
    log_info(logger, "Hard link creado: %s -> %s", nombre_block_logic, nombre_block);

    free(nombre_block); free(pre_ruta); free(ruta_F_block); 
    free(nombre_block_logic); free(ruta_L_block_final);

    return bloque_fisico; 
}

void liberar_bloque_reservado(int nro_bloque) {
    //solo si no hay mas enlaces existentes
    pthread_mutex_lock(&mutex_bitmap);
    
    bitarray_clean_bit(BA_bitmap, nro_bloque);
    log_info(logger,"##%u- Bloque Físico Liberado - Número de Bloque: %u",g_query_id_actual,nro_bloque);
    
    pthread_mutex_unlock(&mutex_bitmap);
}
void limpiar_bloque_fisico(int nro_bloque) {
    char* nombre_block = crear_nombre_block(nro_bloque, 4);
    char* pre_ruta = add_seg_ruta("/physical_blocks", nombre_block);
    char* ruta_F_block = add_seg_ruta(PUNTO_MONTAJE, pre_ruta);

    // "wb" trunca el archivo a 0 y permite escribir
    FILE* f = fopen(ruta_F_block, "wb");
    if (f) {
        // Creamos un buffer de ceros
        char* ceros = calloc(1, BLOCK_SIZE);
        
        // Escribimos ceros en todo el bloque para borrar "fantasmas"
        fwrite(ceros, 1, BLOCK_SIZE, f);
        
        free(ceros);
        fclose(f);
    } else {
        log_error(logger, "No se pudo limpiar el bloque físico %d", nro_bloque);
    }

    free(nombre_block);
    free(pre_ruta);
    free(ruta_F_block);
}
void ocupar_bloque_reservar(int nro_bloque) {
    //solo si no hay mas enlaces existentes
    pthread_mutex_lock(&mutex_bitmap);
    
    bitarray_set_bit(BA_bitmap, nro_bloque);
    
    pthread_mutex_unlock(&mutex_bitmap);
}
char* crear_nombre_block(int valor, int cod) {
    char* nombre = malloc(25); 

    if (!nombre) 
        return NULL;
    if(cod == 4)
    sprintf(nombre, "block%04d.dat", valor);
    else 
    sprintf(nombre, "%06d.dat", valor);

    return nombre;
}
int encontrar_y_reservar_bloque() {
    
    pthread_mutex_lock(&mutex_bitmap);

    int bloque_libre = buscar_primer_bloque_libre(BA_bitmap);

    if (bloque_libre > 0) {
        bitarray_set_bit(BA_bitmap, bloque_libre);
        limpiar_bloque_fisico(bloque_libre);
    }

    pthread_mutex_unlock(&mutex_bitmap); 
    return bloque_libre;
}
int buscar_primer_bloque_libre() {
    
    int cant_bloques = FS_SIZE / BLOCK_SIZE; 
    for (int i = 0; i<cant_bloques; i++) {
        
        if (bitarray_test_bit(BA_bitmap, i) == false) {
            return i;
        }
    }

    log_error(logger, "No se encontró espacio libre en el bitmap.");
    return ERROR_ESPACIO_INSUFICIENTE; 
}
int buscar_num_ultimo_bloque(char* ruta_logical_block){
 // ruta_logical_block es ".../files/FILE/TAG/logical_blocks"
    
    char* ultimo_slash = strrchr(ruta_logical_block, '/');
    if (ultimo_slash == NULL) {
        log_error(logger, "Ruta inválida: %s", ruta_logical_block);
        return ERROR_DESCONOCIDO;
    }

    char* ruta_tag = strndup(ruta_logical_block, ultimo_slash - ruta_logical_block);

    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
     
    t_config* temp = config_create(ruta_metadata);
    if (temp == NULL) {
        log_error(logger, "No se pudo leer metadata en: %s", ruta_metadata);
        free(ruta_tag);
        free(ruta_metadata);
        return ERROR_DESCONOCIDO; 
    }

    int tamaño = config_get_int_value(temp, "TAMAÑO");
    int bloques_actuales = (int)ceil((double)tamaño / (double)BLOCK_SIZE);
    int proximo_bloque = bloques_actuales;
    
    free(ruta_tag);
    free(ruta_metadata);
    config_destroy(temp); 

    return proximo_bloque;  
}
int actualizar_metadata_bloque(char* file, char* tag, int num_L_block_a_cambiar, int nro_bloque_fisico_nuevo) {
    
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);
    char* ruta_tag = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");

    t_config* config = config_create(ruta_metadata);
    if (config == NULL) {
        log_error(logger, "Error al abrir metadata para actualizar: %s", ruta_metadata);
        free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
        return ERROR_DESCONOCIDO;
    }

    char** bloques_array = config_get_array_value(config, "BLOCKS");
    if (bloques_array == NULL) {
        log_error(logger, "Error al leer 'BLOCKS' de metadata: %s", ruta_metadata);
        config_destroy(config);
        free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
        return ERROR_DESCONOCIDO;
    }

    int array_size = 0;
    while (bloques_array[array_size] != NULL) {
        array_size++;
    }

    if (num_L_block_a_cambiar >= array_size) {
        log_error(logger, "Error: num_L_block (%d) está fuera de rango (Tamaño: %d)", num_L_block_a_cambiar, array_size);
        string_array_destroy(bloques_array);
        config_destroy(config);
        free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
        return ERROR_FUERA_DE_LIMITE;
    }

    free(bloques_array[num_L_block_a_cambiar]); 
    
    bloques_array[num_L_block_a_cambiar] = string_itoa(nro_bloque_fisico_nuevo);

    char* joined_string = join_string_array(bloques_array, ","); 
    char* final_array_string = string_from_format("[%s]", joined_string);

    config_set_value(config, "BLOCKS", final_array_string);

    config_save(config);

    free(joined_string);
    free(final_array_string);
    string_array_destroy(bloques_array); 
    config_destroy(config);
    free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);

    log_info(logger, "Metadata actualizada: Bloque lógico %d de %s:%s ahora apunta a físico %d",
             num_L_block_a_cambiar, file, tag, nro_bloque_fisico_nuevo);
    
    return 0;
}
char* join_string_array(char** array, char* separator) {
    
    int size = string_array_size(array);
    
    if (size == 0) {
        return string_new(); // Devuelve un string vacío
    }

    char* resultado = string_duplicate(array[0]);

    for (int i = 1; i < size; i++) {
        
        string_append_with_format(&resultado, "%s%s", separator, array[i]);
    }

    return resultado;
}

int anadir_a_dicc_estado(char* key){
    int estado_op;
    pthread_mutex_lock(&mutex_dic_estado); 

    if(dictionary_has_key(dicc_estado_tag, key)){

        log_error(logger, "Error: Se intentó operar sobre un File:Tag existente: %s", key);
        estado_op = ERROR_FILE_TAG_PREEXISTENTE; 

    }else{
    
    intptr_t estado_ptr = (intptr_t)1; 
    dictionary_put(dicc_estado_tag, key, (void*)(intptr_t)estado_ptr);
    log_info(logger, "File:Tag añadido a diccionario de ESTASDo: %s",key);
        
    estado_op = 0;
    }
    pthread_mutex_unlock(&mutex_dic_estado); 
    return estado_op; 
}
int obtener_estado_file_tag(char* key){
    int estado_final;
    pthread_mutex_lock(&mutex_dic_estado); 

    if(dictionary_has_key(dicc_estado_tag, key)){
    intptr_t estado_tag = (intptr_t)dictionary_get(dicc_estado_tag, key);
    estado_final = (int)estado_tag;
    }else{
        log_error(logger, "Error: Se intentó operar sobre un File:Tag no existente: %s", key);
        estado_final = -1; 
    
    } 
    pthread_mutex_unlock(&mutex_dic_estado); 
    return estado_final; 
}
int actualizar_dicc_estado(char* key_file_tag,int nuevo_estado){
    pthread_mutex_lock(&mutex_dic_estado); 

    if(!dictionary_has_key(dicc_estado_tag, key_file_tag)){
        log_error(logger, "Error: Se intentó actualizar un estado no existente: %s", key_file_tag);
        pthread_mutex_unlock(&mutex_dic_estado);
        return ERROR_FILE_TAG_INEXISTENTE;
    }
    intptr_t estado_ptr = (intptr_t)nuevo_estado; 
    dictionary_put(dicc_estado_tag, key_file_tag, (void*)(intptr_t)estado_ptr);
    log_info(logger, "Estado actualizado para %s a %d", key_file_tag, nuevo_estado);       
    
    pthread_mutex_unlock(&mutex_dic_estado); 
    return 0;
}
int calcular_cant_bloq_log(char* file, char* tag){
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE,"/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);          
    char* ruta_tag  = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");
    t_config* config_tag = config_create(ruta_metadata);

    int tamanio = config_get_int_value(config_tag, "TAMAÑO");
    int cantidad_bloques = tamanio/BLOCK_SIZE; 

    config_destroy(config_tag);
    free(ruta_files);
    free(ruta_file);  
    free(ruta_tag);  
    free(ruta_metadata);  
    return cantidad_bloques;
}
void rollback_falla_incrementar(int* bloques_fisicos_nuevos, int cant_exitosos) {
    
    log_warning(logger, "TRUNCATE: Falló el incremento. Revirtiendo %d bloques del bitmap...", cant_exitosos);

    for (int i = 0; i < cant_exitosos; i++) {
        int nro_bloque_a_liberar = bloques_fisicos_nuevos[i];
        
        log_debug(logger, "Rollback: Liberando bloque físico %d", nro_bloque_a_liberar);
        
        liberar_bloque_reservado(nro_bloque_a_liberar);
    }

    log_info(logger, "Rollback del bitmap completado.");
}
void log_contenido_legible(t_log* logger, const char* prefijo, char* contenido, int tamanio) {
    
    if (contenido == NULL) {
        log_info(logger, "%s (Tamaño %d): [CONTENIDO NULO]", prefijo, tamanio);
        return;
    }
    if (tamanio > MAX_LOG_TEXT_PREVIEW) {
        
        log_info(logger, "%s (Tamaño %d, mostrando %d): %.*s ...[truncado]",
                 prefijo,                     // El mensaje
                 tamanio,                     // El tamaño real
                 MAX_LOG_TEXT_PREVIEW,        // El tamaño que mostramos
                 MAX_LOG_TEXT_PREVIEW,        // El '.*' (cuántos bytes imprimir)
                 contenido);                  // El buffer

    } else {
        
        log_info(logger, "%s (Tamaño %d): %.*s",
                 prefijo,                     // El mensaje
                 tamanio,                     // El tamaño real
                 tamanio,                     // El '.*' (cuántos bytes imprimir)
                 contenido);                  // El buffer
    }
}
void liberar_bloque_si_no_se_usa(int nro_bloque) {
    char* nombre_block = crear_nombre_block(nro_bloque, 4);
    char* pre_ruta = add_seg_ruta("/physical_blocks", nombre_block);
    char* ruta_F_block = add_seg_ruta(PUNTO_MONTAJE, pre_ruta);

    struct stat st_fisico;
    if (stat(ruta_F_block, &st_fisico) == -1) {
        log_error(logger, "Error en stat de %s al liberar: %s", ruta_F_block, strerror(errno));
    } else {
        // nlink == 1 significa que solo el propio archivo en /physical_blocks lo apunta.
        // Nadie más lo está usando.
        if (st_fisico.st_nlink == 1) {
           // log_info(logger, "BITMAP: Bloques disponibles: %u. Bloques ocupados: %u", );
            log_info(logger, "BITMAP: Bloque %d (nlink=1) ya no se usa. Liberando en bitmap.", nro_bloque);
            liberar_bloque_reservado(nro_bloque); // Libera en tu bitmap
            // Opcional: unlink(ruta_F_block) para borrar el archivo físico
        }
    }

    free(nombre_block);
    free(pre_ruta);
    free(ruta_F_block);
}
int actualizar_metadata_incremento(char* file, char* tag, int* bloques_fisicos_nuevos, int cant_bloques_a_agregar) {
    
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);
    char* ruta_tag = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");

    t_config* config = config_create(ruta_metadata);
    if (config == NULL) {
        log_error(logger, "TRUNCATE: Error al abrir metadata: %s", ruta_metadata);
        free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
        return ERROR_DESCONOCIDO;
    }

    char** bloques_actuales_str = config_get_array_value(config, "BLOCKS");
    int cant_actual = string_array_size(bloques_actuales_str);
    int cant_total = cant_actual + cant_bloques_a_agregar;

    // Crear array combinado
    char** bloques_totales_str = malloc((cant_total + 1) * sizeof(char*));

    // Copiar viejos
    for (int i = 0; i < cant_actual; i++) {
        bloques_totales_str[i] = string_duplicate(bloques_actuales_str[i]);
    }

    // Copiar nuevos
    for (int i = 0; i < cant_bloques_a_agregar; i++) {
        bloques_totales_str[cant_actual + i] = string_itoa(bloques_fisicos_nuevos[i]);
    }
    bloques_totales_str[cant_total] = NULL; 

    // Guardar
    char* joined_string = join_string_array(bloques_totales_str, ","); 
    char* final_array_string = string_from_format("[%s]", joined_string);

    config_set_value(config, "BLOCKS", final_array_string);
    config_save(config);

    free(joined_string);
    free(final_array_string);
    string_array_destroy(bloques_actuales_str); 
    string_array_destroy(bloques_totales_str); 
    config_destroy(config);
    free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
    
    return 0;
}
int actualizar_metadata_decremento(char* file, char* tag, int cant_bloques_final) {
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);
    char* ruta_tag = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");

    t_config* config = config_create(ruta_metadata);
    if (!config) {
       log_error(logger, "TRUNCATE: Error al abrir metadata: %s", ruta_metadata);
        free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
        return ERROR_DESCONOCIDO;
        
    }

    char** bloques_array = config_get_array_value(config, "BLOCKS");
    
    // Aquí está el truco: Forzamos un NULL en la nueva posición final
    // para "cortar" el array.
    if (bloques_array[cant_bloques_final] != NULL) {
        
        int j = cant_bloques_final;
        while(bloques_array[j] != NULL) {
            free(bloques_array[j]);
            bloques_array[j] = NULL; // Cortamos aquí
            j++;
        }
    }

    // Reconstruimos el string: [1,2,3]
    char* joined = join_string_array(bloques_array, ",");
    char* final_str = string_from_format("[%s]", joined);

    config_set_value(config, "BLOCKS", final_str);
    config_save(config);

    free(joined); free(final_str);
    string_array_destroy(bloques_array); 
    config_destroy(config);
    free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
    return 0;
}
int obtener_nro_bloque_fisico(char* file, char* tag, int num_L_block) {
    char* ruta_files = add_seg_ruta(PUNTO_MONTAJE, "/files");
    char* ruta_file = add_seg_ruta(ruta_files, file);
    char* ruta_tag = add_seg_ruta(ruta_file, tag);
    char* ruta_metadata = add_seg_ruta(ruta_tag, "/metadata.config");

    t_config* config = config_create(ruta_metadata);
    if (config == NULL) {
         log_error(logger, "No se pudo leer metadata de %s", ruta_metadata);
        free(ruta_files);free(ruta_file);
        free(ruta_tag);free(ruta_metadata);        
        return ERROR_DESCONOCIDO;
        
    }

    char** blocks = config_get_array_value(config, "BLOCKS");
    int nro_fisico = -1;

    // Validamos que el índice exista
    int count = 0;
    while(blocks[count] != NULL) count++;

    if (num_L_block < count) {
        nro_fisico = atoi(blocks[num_L_block]);
    }

    string_array_destroy(blocks);
    config_destroy(config);
    
    free(ruta_files); free(ruta_file); free(ruta_tag); free(ruta_metadata);
    
    return nro_fisico;
}
void destruir_elemento_mutex(void* elemento) {
    pthread_mutex_t* mutex = (pthread_mutex_t*) elemento;
    pthread_mutex_destroy(mutex); 
    free(mutex);
}

void limpiar_y_terminar() {
    log_warning(logger, "Iniciando protocolo de cierre...");

    if (server_fd_general > 0) {
        close(server_fd_general);
    }

    // 2. Persistencia del BITMAP (¡Lo más importante!)
    if (mmap_BM != NULL) {
        // Calculamos tamaño en bytes
        size_t tam_bitmap = ((FS_SIZE / BLOCK_SIZE + 7) / 8);
        
        // Forzamos escritura a disco (Sincronización)
        if (msync(mmap_BM, tam_bitmap, MS_SYNC) == -1) {
            log_error(logger, "Error sincronizando Bitmap a disco");
        } else {
            log_info(logger, "Bitmap sincronizado a disco correctamente.");
        }
        
        // Liberamos struct de commons y mapeo
        if (BA_bitmap) 
            bitarray_destroy(BA_bitmap);
        munmap(mmap_BM, tam_bitmap);
    }

    // 3. Limpiar Diccionario de Mutexes (File:Tag)
    if (file_tag_dic != NULL) {
        dictionary_destroy_and_destroy_elements(file_tag_dic, destruir_elemento_mutex);
    }

    // 4. Limpiar Diccionario de Estados
    if (dicc_estado_tag != NULL) {
        // Como usamos (void*)(intptr_t) para guardar enteros, NO hay mallocs dentro.
        // Solo destruimos el diccionario contenedor.
        dictionary_destroy(dicc_estado_tag);
    }

    // 5. Destruir Mutexes Globales
    pthread_mutex_destroy(&mutex_bitmap);
    pthread_mutex_destroy(&mutex_dir_files);
    pthread_mutex_destroy(&mutex_file_hash);
    pthread_mutex_destroy(&mutex_diccionary);
    pthread_mutex_destroy(&mutex_dic_estado);
    pthread_mutex_destroy(&mutex_cant_workers);

    // 6. Liberar Configs globales
    if (config) config_destroy(config);
    if (sp_block_config) config_destroy(sp_block_config); // (Si quedó abierto)
    if (config_hash) config_destroy(config_hash);

    log_info(logger, "Storage finalizado correctamente.");
    
    // 7. Liberar Logger (Lo último, para poder loguear lo anterior)
    if (logger) log_destroy(logger);
}

// Tu Handler de Señales
void sighandler(int s) {
    limpiar_y_terminar();
    exit(EXIT_SUCCESS);
}