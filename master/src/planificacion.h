#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/temporal.h>

#include "./utils/utils.h"
#include "./utils/empaquetar.h"
#include "./utils/desempaquetar.h"
#include "sync.h"
#include "queries.h"
#include "worker_conexion.h"

#define VALOR_NULO_EVENTO -1

// Agregar a planificacion.h
typedef enum tipoEvento {
    EVENTO_WORKER_DESCONECTADO,
    EVENTO_QUERY_CONTROL_DESCONECTADO,
    EVENTO_NUEVA_QUERY,
    EVENTO_WORKER_LIBERADO,
    EVENTO_NUEVO_WORKER_CONECTADO,
    EVENTO_AGING_OCURRIDO
} t_tipo_evento;

typedef struct 
{
    t_tipo_evento tipo;
    uint32_t worker_id;
    uint32_t query_id;
    uint32_t program_counter;
} t_evento_planificacion;
typedef struct 
{
    t_query* query;
    uint64_t tiempo_llegada;
    uint32_t prioridad_efectiva;
    uint64_t ultimo_aging;  
}t_elemento_cola;


void inicializar_listas_planificacion();
uint64_t timestamp_actual_en_milisegundos();
t_query* crear_nuevo_query(char* query_path, uint32_t prioridad, int conexion);
t_elemento_cola* crear_nuevo_elemento(t_query* query);
t_elemento_cola* buscar_y_remover_por_qid(t_list* lista, uint32_t qid);

void *main_planificacion();
void planificar_por_fifo();
void intentar_asignaciones_fifo();

void planificar_por_prioridades();
void intentar_asignaciones_prioridades();
t_elemento_cola* obtener_query_mas_prioritaria_mas_antigua();
t_elemento_cola* obtener_victima_desalojo(uint32_t prioridad_desalojador);
void agregar_query_ordenada(t_list* lista, t_elemento_cola* elemento);
uint32_t solicitar_desalojo_bloqueante(t_worker_conectado* worker_a_desalojar, uint32_t query_id);
void asignar_query_a_worker(t_elemento_cola* elemento, t_worker_conectado* worker);

void* main_aging(void* args);
void dormir_milisegundos(int tiempo);
bool aplicar_aging_inteligente();
void verificar_y_aplicar_aging_si_corresponde();


void* manejar_eventos_planificacion(void* args);
void inicializar_cola_eventos();
void enviar_evento_planificacion(t_tipo_evento tipo, uint32_t worker_id, uint32_t query_id, uint32_t qc_id);
void manejar_worker_desconectado(uint32_t worker_id, uint32_t query_id_ejecutando);
void manejar_query_control_desconectado(uint32_t query_id_activo);
void worker_libera_query(uint32_t worker_id, uint32_t query_id, uint32_t pc);


#endif /* PLANIFICACION_H_ */