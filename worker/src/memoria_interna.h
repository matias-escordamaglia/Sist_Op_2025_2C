#ifndef MEMORIA_INTERNA_H
#define MEMORIA_INTERNA_H

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "utilsWorker.h"
#include <errno.h>

t_tabla_paginas* obtener_o_crear_tp(char* file, char* tag);
t_tabla_paginas* buscar_en_lista_global(const char* file, const char* tag);


#endif // MEMORIA_INTERNA_H
