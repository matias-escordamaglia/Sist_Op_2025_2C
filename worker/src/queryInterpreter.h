#ifndef QUERYINTERPRETER_H
#define QUERYINTERPRETER_H

#include <ctype.h>
#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "utilsWorker.h"
#include "memoria_interna.h"
#include <errno.h>
#include <ctype.h>


void* main_lanzamiento_ejecucion();
bool parsear_read_params(char* params, t_read* out);
void envioAQueryInterpreter();
char* instruccion_n(char* nombre, size_t idx);
t_programa* obtener_programa(char* nombre);
char* const* instrucciones_desde(char* nombre, size_t idx_1based, size_t* out_cant);
char* saltar_blancos(char* p);
bool empieza_con(char* s, char* kw);
int ejecutar_create(t_create* c, uint32_t queryid);
void ejecutarOperacion(char* const* instrucciones, size_t cantidad);
bool ejecutar_linea(char* linea, uint32_t query_id);
void destruir_create(t_create* c);
bool parsear_create_params(char* params, t_create* out);
bool detectar_operacion(char* linea, Operation* out_op, char** out_params);
int enviar_create_a_storage(int conexion, char* file,   char* tag, uint32_t Op);
int ejecutar_truncate(t_truncate* c,uint32_t queryid);
int enviar_truncate_a_storage(int conexion,   char* file,   char* tag, size_t tam);
bool parsear_truncate_params(  char* params, t_truncate* out);
bool parsear_tag_params(char* params, t_tag* out);
int ejecutar_tag( t_tag* t, uint32_t queryid);
int enviar_tag_a_storage(int conexion,  char* file_origen,   char* tag_origen,   char* file_dest,  char* tag_dest);
bool parsear_write_params(char* params, t_write* out);
void destruir_truncate(t_truncate* c);
void destruir_tag(t_tag* t);
int recibir_respuesta_storage(int conexion, t_log* logger);
int ejecutar_commit(t_create* c,uint32_t queryid);
int ejecutar_delete(t_create* c, uint32_t queryid);
void enviar_lectura_a_master(char* file, char* tag, void* contenido, uint32_t tamanio);
void flush_file_tag_en_memoria(char* file, char* tag, uint32_t id_query);
void finalizar_query_con_error(int motivo);
char* storage_error_to_string(int motivo);
void destruir_write(t_write* w);
void destruir_read(t_read* r);
void destruir_create(t_create* c);
void destruir_tag(t_tag* t);
void desconectarseDeStorage();

#endif // QUERYINTERPRETER_H
