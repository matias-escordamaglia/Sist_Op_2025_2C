#ifndef UTILS_H_
#define UTILS_H_

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>




// ------------------------------------------------------------------------------------------
// ----------NO ELIMINAR LAS SIGUIENTES COSAS, ESTAN RELACIONADAS CON CONEXIONES-------------
// ------------------------------------------------------------------------------------------

#define HANDSHAKE_WORKER_MASTER 1
#define HANDSHAKE_QUERY_MASTER 2

typedef enum
{
	MENSAJE,
	PAQUETE
} op_code;

typedef enum {
    HANDSHAKE_OK,
    HANDSHAKE_FALLO
} t_estado_handshake;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;


int crear_conexion(char* ip, char* puerto,t_log* logger);
int recibir_operacion(int socket_cliente, t_log* logger);
int iniciar_servidor(char* ip, char* puerto, t_log* logger);
int esperar_cliente(int socket_servidor, t_log* logger);

t_paquete* crear_paquete(void);
uint32_t handshake(int conexion, uint32_t envio, t_log* logger, char *modulo);
uint32_t handshake_silencioso(int conexion, uint32_t envio, t_log* logger, char *modulo);
t_config* iniciar_config(t_log* logger, char* modulo);
t_config* iniciar_config_vieja(t_log* logger, char* modulo); //funcion del tp pasado
/** 
* @brief Obtiene un LOG_LEVEL de un archivo de config
* @param config Archivo de configuración que debe tener el Módulo/Hilo
* @return t_log_level
*/
t_log_level obtener_log_level_config(t_config* config);
t_list* recibir_paquete(int socket_cliente, t_log* logger);

void enviar_mensaje(char* mensaje, int socket_cliente);
void recibir_mensaje(int socket_cliente, t_log* logger);
char* recibir_y_devolver_mensaje(int socket_cliente, t_log* logger);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void* recibir_buffer(int* size, int socket_cliente);
void* recibir_buffer_con_logger(int* size, int socket_cliente, t_log* logger);
void crear_buffer(t_paquete* paquete);


// ------------------------------------------------------------------------------------------
// -- Enums --
// ------------------------------------------------------------------------------------------

typedef enum {
    OK,
    ERROR,
} t_resultado_operacion_default;

typedef enum Operation{
    CREATE,
    TRUNCATE,
    WRITE,
    READ,
    TAG,
    COMMIT,
    FLUSH,
    DELETE,
    END
} Operation;


typedef enum MotivoMasterWorker{
    PEDIDO_QUERY,
    INTERRUPCION
} t_motivo_pedido_master_worker;

typedef enum TipoAvisoMasterWorker{
    FINALIZACION_QUERY,
    NUEVA_LECTURA,
    DEVOLUCION_X_INTERRUPCION
} t_tipo_aviso_worker_master;

typedef enum TipoAvisoMasterQuery{
    LECTURA_QUERY,
    QUERY_FINALIZADO
} t_motivo_aviso_master_query;


// ------------------------------------------------------------------------------------------
// -- Structs --
// ------------------------------------------------------------------------------------------


typedef struct {
    uint32_t numeroA;
    uint32_t numeroB;
    char* string;
}t_prueba_conexion;


typedef struct 
{
    uint32_t prioridad;
    char* path_query;
}t_pedido_query_master;

typedef struct
{
    t_motivo_aviso_master_query motivo;
    char* file_tag;
    char* mensaje;
}t_aviso_master_query;


typedef struct
{
    t_motivo_pedido_master_worker motivo;
    uint32_t query_id;
    uint32_t program_counter;
    char* query_path;
}t_pedido_master_worker;

typedef struct {
    t_tipo_aviso_worker_master tipo_aviso;
    char* argumento;
}t_aviso_worker_master;


// ------------------------------------------------------------------------------------------
// -- Funciones --
// ------------------------------------------------------------------------------------------

void list_iterate_with_data(t_list* lista, void (*func)(void*, void*), void* extra);

#endif /* UTILS_H_ */