#ifndef UTILSWORKER_H
#define UTILSWORKER_H

#include <stdlib.h>
#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include <dirent.h>
#include <semaphore.h>
#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"

#define QID_NULO -1
#define PC_NULO -1


typedef struct 
{
    u_int32_t qid_actual;
    u_int32_t pc_actual;
    char* query_path;
} t_query;

extern t_query query_actual;

extern int conexion_storage;
extern int conexion_master;
extern t_log* logger;
extern t_dictionary* diccionario_programas;
extern u_int32_t QID_actual;
extern bool hay_pedido_desalojo;



extern sem_t* sem_desalojo_pendiente;
extern sem_t* sem_ejecucion_pendiente;



void iniciar_semaforos();
void destruir_semaforos();

void settear_valores_nulos_query_actual();
void deterner_ejecucion_query_segun_motivo_y_mensaje(t_tipo_aviso_worker_master tipo, char* mensaje);
void deterner_ejecucion_query_finalizado();
void deterner_ejecucion_query_error(char* mensaje);

#endif /*UTILSWORKER_H*/