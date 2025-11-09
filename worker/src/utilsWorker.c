#include "utilsWorker.h"

int conexion_storage = 0;
int conexion_master = 0;
t_log* logger = NULL;
t_dictionary* diccionario_programas = NULL;
uint8_t* MEM               = NULL;  
size_t   TAM_PAGINA        = 4096;
int      RETARDO_MEMORIA_MS= 5;  
int block_size = 1;