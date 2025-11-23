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
int escritura_bloque(char*, char*, int, char*,int);
char* lectura_bloque(char*, char*, int,int*  );
int eliminar_tag(char*,char*);


int obtener_tamano(char*);
int incrementar(char*file,char*tag,int, int, char*);
int decrementar(int, int, char*);
void copiar_archivo(char*, char*);
int copiar_directorio(char*, char*);
int duplicar_enlaces_bloques(char* ruta_tag_origen, char* ruta_tag_destino);
void eliminar_directorio(char*);
int bloq_L_apuntan_bloq_F_0(char*);
int procesar_bloque_logico(char*, int);



#endif /* OPERACIONES_H_ */