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
void truncar_archivo(char*, char*, char*);
void tag_file(char*, char*);
void commit_tag(char* , char* );
void escritura_bloque();
void lectura_bloque();
void eliminar_tag();


int obtener_tamano(char*);
void incrementar(int, int, char*);
void decrementar(int, int);
void copiar_archivo(char*, char*);
void copiar_directorio(char*, char*);
void eliminar_directorio(char*);
int bloq_L_apuntan_bloq_F_0(char*);
void recorrer_logical_blocks(char*);
void procesar_bloque_logico(char*, int);

#endif /* OPERACIONES_H_ */