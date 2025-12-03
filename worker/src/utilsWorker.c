#include "utilsWorker.h"

int conexion_storage = 0;
int conexion_master = 0;
t_log* logger = NULL;
t_dictionary* diccionario_programas = NULL;
u_int32_t QID_actual = QID_NULO; //Inicia en -1 dado que QID puede ser 0 si es el primero
bool hay_pedido_desalojo = false;

uint8_t* MEM               = NULL;  
size_t   TAM_PAGINA        = 4096;
int      RETARDO_MEMORIA_MS= 5;  
int block_size = 1;
pthread_mutex_t mutex_mem = PTHREAD_MUTEX_INITIALIZER;

sem_t* sem_desalojo_pendiente;
sem_t* sem_ejecucion_pendiente;

t_query query_actual = { .qid_actual = QID_NULO, .pc_actual = PC_NULO, .query_path = NULL };

void iniciar_semaforos() {
	
	sem_desalojo_pendiente= malloc(sizeof(sem_t)); 
    sem_ejecucion_pendiente = malloc(sizeof(sem_t));

    sem_init(sem_desalojo_pendiente, 0, 0);
	sem_init(sem_ejecucion_pendiente, 0, 0);

}	

void destruir_semaforos() {
	
	sem_destroy(sem_desalojo_pendiente);
	sem_destroy(sem_ejecucion_pendiente);

}

void settear_valores_nulos_query_actual() {
	query_actual.qid_actual = QID_NULO;
	query_actual.pc_actual = PC_NULO;
	query_actual.query_path = "Sin query actualmente";
}

void deterner_ejecucion_query_segun_motivo_y_mensaje(t_tipo_aviso_worker_master tipo, char* mensaje) {

    t_aviso_worker_master* aviso_detencion = malloc(sizeof(t_aviso_worker_master));

    aviso_detencion->tipo_aviso = tipo;
	if (mensaje == NULL) {
		int pc_temp = query_actual.pc_actual;
		aviso_detencion->argumento = convertir_int_a_string(pc_temp);
	} else {
		aviso_detencion->argumento = mensaje;
	}

    settear_valores_nulos_query_actual();

    t_paquete* paquete = empaquetar_aviso_worker_master(aviso_detencion);

    enviar_paquete(paquete, conexion_master);

    free(aviso_detencion->argumento);
    free(aviso_detencion);
}

void deterner_ejecucion_query_finalizado()
{
	deterner_ejecucion_query_segun_motivo_y_mensaje(FINALIZACION_QUERY, NULL);
}

void detener_ejecucion_query_error(char* mensaje)
{
	deterner_ejecucion_query_segun_motivo_y_mensaje(ERROR_QUERY, mensaje);
}

