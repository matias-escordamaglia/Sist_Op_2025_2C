#ifndef UTILSWORKER_H
#define UTILSWORKER_H

#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include <dirent.h>
#include <stdint.h>
#include <stddef.h> 
#include <pthread.h>


extern int conexion_storage;
extern int conexion_master;
extern t_log* logger;
extern t_dictionary* diccionario_programas;
extern uint8_t* MEM;                  // único malloc
extern size_t   TAM_PAGINA;
extern int      RETARDO_MEMORIA_MS;
extern int block_size;
extern pthread_mutex_t mutex_mem;


#endif /*UTILSWORKER_H*/