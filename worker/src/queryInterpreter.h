#ifndef QUERYINTERPRETER_H
#define QUERYINTERPRETER_H

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "utilsWorker.h"


void envioAQueryInterpreter(t_pedido_master_worker* pedido);
const char* instruccion_n(const char* nombre, size_t idx);
t_programa* obtener_programa(const char* nombre);
const char* const* instrucciones_desde(const char* nombre, size_t idx_1based, size_t* out_cant);
char* saltar_blancos(const char* p);
bool empieza_con(const char* s, const char* kw);
bool ejecutar_create(const t_create* c);
void ejecutarOperacion(t_pedido_master_worker* pedido, const char* const* instrucciones, size_t cantidad);
bool ejecutar_linea(const char* linea);
void destruir_create(t_create* c);
bool parsear_create_params(const char* params, t_create* out);
bool detectar_operacion(const char* linea, Operation* out_op, const char** out_params);
int enviar_create_a_storage(int conexion, const char* file, const char* tag);


#endif // QUERYINTERPRETER_H
