#ifndef OPERACIONES_H_
#define OPERACIONES_H_

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
#include <fcntl.h>
#include <sys/mman.h>

#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"
#include <commons/bitarray.h>


int create(char* , char*);
int truncar_archivo(char*, char*, int);
int tag_file(char*, char*);
int commit_tag(char* , char* );
int escritura_bloque(char*, char*, int, char*);
char* lectura_bloque(char*, char*, int);
void eliminar_tag();


int obtener_tamano(char*);
void incrementar(int, int, char*);
void decrementar(int, int);
void copiar_archivo(char*, char*);
void copiar_directorio(char*, char*);
void eliminar_directorio(char*);
int bloq_L_apuntan_bloq_F_0(char*);
void recorrer_logical_blocks(char*, char*);
int procesar_bloque_logico(char*, int);
void eliminar_block_metadata(char*, int);

#endif /* OPERACIONES_H_ */