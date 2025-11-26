#ifndef STORAGE_H_
#define STORAGE_H_
#define MAX_LOG_TEXT_PREVIEW 256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>

#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"
#include <commons/bitarray.h>
#include <commons/collections/dictionary.h>
#include <commons/string.h>



extern t_log *logger;
extern t_config *config;
extern t_config *sp_block_config; 
//get configs
extern char* PUERTO_ESCUCHA; 
extern bool FRESH_START;
extern char* PUNTO_MONTAJE;
extern int RETARDO_OPERACION;
extern int RETARDO_ACCESO_BLOQUE; 

//hash config
extern t_config* config_hash; 

extern int BLOCK_SIZE; 
extern int FS_SIZE; 

// bitarray
extern char* mmap_BM;
extern t_bitarray* BA_bitmap; 

//semaforos mutex
extern pthread_mutex_t mutex_bitmap;
extern pthread_mutex_t mutex_dir_files; 
extern pthread_mutex_t mutex_file_hash;
extern pthread_mutex_t mutex_diccionary; 
extern pthread_mutex_t mutex_dic_estado; 
//dictionarys
extern t_dictionary* file_tag_dic ; 
extern t_dictionary* dicc_estado_tag; 

// query_id por hilo
extern __thread int g_query_id_actual;






extern t_log_level log_level;
void extraer_storage_config(t_config* config);
char* add_seg_ruta(char *, char *);
bool existe_archivo(char *);
void iniciar_estructuras(char*);
bool existe_archivo(char *);
char* add_seg_ruta(char *, char *);
int existe_directorio(const char *);
int borrar_directorio(const char *);
void inicializar_super_block_config(char*);
void inicializar_dir_physic_block(char* );
void inicializar_bitmap(const char* );
void crear_bloque(const char* , size_t );
void inicializar_dir_logic_block( char* );
void inicializar_blocks_hash(char* );
void inicializar_dictionary_mutex();
void crear_metadata_config(char* );
int busqueda_block_asociado_hash(char* );
void inicializar_semaforos();
void iniciar_mutex_file_tag(char* );
void eliminar_mutex_file_tag(char* );
void cargar_estructuras_existentes(char* );
void cargar_block_hash(char* ); //config_create
void cargar_bitmap(char* );
void mapeo_dir_mutex_dinamic(char* );
char* crear_key_file_tag(char* ,  char*);
int lectura_metadata(char* );
void finalizar_munmap();
int asignar_bloque_logico(char* ruta_logical_block);
void liberar_bloque_reservado(int nro_bloque); //bitmap
void ocupar_bloque_reservar(int nro_bloque) ; //bitmap
char* crear_nombre_block(int valor, int cod);
int encontrar_y_reservar_bloque();
int buscar_primer_bloque_libre();
int buscar_num_ultimo_bloque(char* ruta_logical_block);
int obtener_estado_file_tag(char* key);
int anadir_a_dicc_estado(char* key);
int actualizar_dicc_estado(char* key_file_tag,int nuevo_estado);
int calcular_cant_bloq_log(char* file, char* tag);
int actualizar_metadata_bloque(char* file, char* tag, int num_L_block_a_cambiar, int nro_bloque_fisico_nuevo);
char* join_string_array(char** array, char* separator); 
void rollback_falla_incrementar(int* bloques_fisicos_nuevos, int cant_exitosos);
int asignar_bloque_logico_especifico(char* ruta_logical_block, int num_bloque_logico);
void log_contenido_legible(t_log* logger, const char* prefijo, char* contenido, int tamanio);
void liberar_bloque_si_no_se_usa(int nro_bloque);
int actualizar_metadata_incremento(char* file, char* tag, int* bloques_fisicos_nuevos, int cant_bloques_a_agregar);
int actualizar_metadata_decremento(char* file, char* tag, int cant_bloques_final); 
int obtener_nro_bloque_fisico(char* file, char* tag, int num_L_block);









#endif /* STORAGE_H_ */