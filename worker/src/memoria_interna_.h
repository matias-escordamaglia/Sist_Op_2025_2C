#ifndef MEMORIA_INTERNA__H
#define MEMORIA_INTERNA__H

#include <commons/config.h>
#include <commons/log.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <./utils/utils.h>

void pasar_bloque_a_memoria(int* block_size);
void inicializar_memoria_interna();
void liberar_memoria_interna();

// Funciones del algoritmo CLOCK
void acceder_a_pagina(int nro_pagina);
//int reemplazar_pagina_clock();

#endif