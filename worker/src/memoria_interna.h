#ifndef MEMORIA_INTERNA_H
#define MEMORIA_INTERNA_H

#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/bitarray.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "utilsWorker.h"


// Funciones de interacción con Storage
int escribir_pagina_a_storage(t_entrada_pagina* victima, uint32_t id_query);
int cargar_pagina_desde_storage(t_tabla_paginas* tabla, uint32_t nro_pagina, int marco_asignado, uint32_t id_query);
void devolver_marco(int marco);
void liberar_marco_de_victima(t_entrada_pagina* victima, uint32_t id_query);
int asignar_marco_o_reemplazar(t_entrada_pagina** victima, uint32_t id_query);
// void insertar_bytes_a_paquete(t_paquete* paquete, void* datos, int tamanio);

void iniciar_memoria_interna(t_config* config);
void destroy_memoria_interna(void);
int memoria_write(t_write* w, uint32_t id_query);
int acceder_memoria(char* file, char* tag,uint32_t dir_base, void *buffer, uint32_t tamanio,bool es_write, uint32_t id_query);
int memoria_read(t_read* r, void* buffer_destino, uint32_t id_query);
void memoria_flush(char *file_tag, uint32_t query_id);
void memoria_flush_all(uint32_t query_id);
void memoria_update_tam_file(char *file_tag, uint32_t nuevo_tam);
void free_tabla(void *elem);
t_tabla_paginas* obtener_o_crear_tabla(char* file, char* tag);
t_tabla_paginas* buscar_en_lista_global(char* file, char* tag);
t_entrada_pagina* get_entry(t_tabla_paginas* tabla, uint32_t nro_pagina);
uint32_t direccion_fisica(uint32_t marco, uint32_t offset, uint32_t tam_p);
t_entrada_pagina* indico_entrada_presente(t_tabla_paginas* tabla, uint32_t nro_pagina, int marco, uint32_t id_query);
void crear_y_agregar_tabla_a_lista_global(char* file, char* tag);
bool rango_valido( t_tabla_paginas* tabla, uint32_t base, uint32_t tam);
void recorrido_iniciar(segmento_acceso* seg, uint32_t base, uint32_t tam, uint32_t tam_p);
int asegurar_pagina_presente(t_tabla_paginas* tabla, uint32_t nro_pagina, uint32_t id_query, t_entrada_pagina** entrada_pagina);
int asignar_marco_o_reemplazar(t_entrada_pagina** victima, uint32_t id_query);
void aplicar_retardo_memoria(uint32_t milis);
void escribir_en_memoria(uint32_t dir_fisica, void* src, uint32_t nbytes);
void marcar_modificada(t_entrada_pagina* e);
void recorrido_siguiente(segmento_acceso* seg, uint32_t tam_pagina);
void actualizar_reemplazo(t_entrada_pagina* e);
t_entrada_pagina* reemplazar_pagina_lru();
t_entrada_pagina* buscar_entrada_por_marco(uint32_t marco_num);
t_entrada_pagina* reemplazar_pagina_clock();
void actualizar_tam_memoria(char* file, char* tag, uint32_t nuevo_tamanio);
void flush_total(int query_id); 



#endif