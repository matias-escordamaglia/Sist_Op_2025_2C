#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"

#include "manejo-worker.h"

//configs
char* PUERTO_ESCUCHA; 
bool FRESH_START;
char* PUNTO_MONTAJE;
int RETARDO_OPERACION;
int RETARDO_ACCESO_BLOQUE; 

int BLOCK_SIZE; 
int FS_SIZE; 


t_log_level log_level;
void extraer_storage_config(t_config* config);
char* add_seg_ruta(char *, char *);
bool existe_archivo(char *);
void iniciar_estructuras();
bool existe_archivo(char *);
char* add_seg_ruta(char *, char *);
int existe_directorio(const char *);
int borrar_directorio(const char *);
void inicializar_super_block_config();
void inicializar_dir_physic_block(char* );
void inicializar_bitmap(const char* );
void crear_bloque(const char* , size_t );
void inicializar_dir_logic_block( char* );




#endif /* STORAGE_H_ */