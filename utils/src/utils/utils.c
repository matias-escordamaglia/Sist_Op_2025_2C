#include "utils.h"


// ------------------------------------------------------------------------------------------
// --------------------NO ELIMINAR LAS SIGUIENTES FUNCIONES----------------------------------
// ------------------------------------------------------------------------------------------

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void* magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(char *ip, char* puerto, t_log* logger)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Crear socket
	int socket_cliente = socket(server_info->ai_family,
	                            server_info->ai_socktype,
	                            server_info->ai_protocol);

	// Conectar
	if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) != 0) {
		log_error(logger, "No se pudo conectar con el servidor");
		freeaddrinfo(server_info);
		return -1;
	}

	log_info(logger, "¡Conectado al servidor!");

	freeaddrinfo(server_info);

	return socket_cliente;
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void)
{
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = PAQUETE;
    crear_buffer(paquete);
    return paquete;
}



void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}


int iniciar_servidor(char* ip, char* puerto, t_log* logger){
	int socket_servidor;
	struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(ip, puerto, &hints, &servinfo) != 0) {
        log_error(logger, "Error en getaddrinfo");
        return -1;
    }

    socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    listen(socket_servidor, SOMAXCONN);
    freeaddrinfo(servinfo);
    log_trace(logger, "Listo para escuchar a mi cliente");

    return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log* logger)
{
	struct sockaddr_in direccion_cliente;
	socklen_t tamanio_direccion = sizeof(struct sockaddr_in);

	int socket_cliente = accept(socket_servidor, (void*)&direccion_cliente, &tamanio_direccion);

	if (socket_cliente == -1) {
		log_error(logger, "Error al aceptar conexión del cliente.");
		return -1;
	}

	log_info(logger, "¡Se conectó un cliente!");

	return socket_cliente;
}

int recibir_operacion(int socket_cliente, t_log* logger)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0){
		log_info(logger, "Código de operación recibido: %d", cod_op);
		return cod_op;
	}
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	if (recv(socket_cliente, size, sizeof(int), MSG_WAITALL) <= 0) {
        return NULL;
	}

    buffer = malloc(*size);

    if (recv(socket_cliente, buffer, *size, MSG_WAITALL) <= 0) {
        free(buffer);
        return NULL;
    }

	return buffer;
}

void recibir_mensaje(int socket_cliente, t_log* logger)
{
    int size;
    char* buffer = recibir_buffer(&size, socket_cliente);

    // Verificar si la recepción fue exitosa antes de proceder
    if (buffer == NULL || size <= 0) {
        log_error(logger, "Error al recibir el mensaje.");
        return;
    }

    log_info(logger, "Me llegó el mensaje: %s", buffer);
    free(buffer); // Liberamos el buffer una vez que ya lo procesamos
}

/*
Se debe hacer free luego de usar el char* devuelto
*/
char* recibir_y_devolver_mensaje(int socket_cliente, t_log* logger)
{
    int size;
    char* buffer = recibir_buffer(&size, socket_cliente);

    if (buffer == NULL || size <= 0) {
        log_error(logger, "Error al recibir el mensaje.");
        return NULL;
    }

    log_info(logger, "Me llegó el mensaje: %s", buffer);
    return buffer;  
}

t_list* recibir_paquete(int socket_cliente, t_log* logger)
{
    int size;
    int desplazamiento = 0;
    void* buffer;
    t_list* valores = list_create();
    int tamanio;

    buffer = recibir_buffer(&size, socket_cliente);

    // Verificar si la recepción fue exitosa antes de proceder
    if (buffer == NULL || size <= 0) {
        log_error(logger, "Error al recibir el paquete.");
        list_destroy(valores);  // Liberamos la lista en caso de error
        return NULL;
    }

    while (desplazamiento < size) {
        memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);
        char* valor = malloc(tamanio);
        memcpy(valor, buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;
        list_add(valores, valor);
    }

    free(buffer);
    return valores;
}

uint32_t handshake(int conexion, uint32_t envio, t_log* logger, char *modulo){
	uint32_t result;

	send(conexion, &envio, sizeof(uint32_t), 0);
	recv(conexion, &result, sizeof(uint32_t), MSG_WAITALL);

	if(result == 0) {
		log_info(logger, "Conexion establecida con %s", modulo);
	} else {
		log_error(logger, "Error en la conexión con %s", modulo);
		return -1;
	}

	return result;
}

uint32_t handshake_silencioso(int conexion, uint32_t envio, t_log* logger, char *modulo){
	uint32_t result;

	send(conexion, &envio, sizeof(uint32_t), 0);
	recv(conexion, &result, sizeof(uint32_t), MSG_WAITALL);

	if(result != 0) {
		log_error(logger, "Error en la conexión con %s", modulo);
		return -1;
	} 

	return result;
}

t_config* iniciar_config(t_log* logger, char* modulo)
{	
	char* reddir_configs = "./configs/";

	char* path = malloc(strlen(reddir_configs) + strlen(modulo) + 1);
    if (path == NULL) {
        log_error(logger, "Error al asignar memoria para la ruta.");
        return NULL;
    }
    
    strcpy(path, reddir_configs);
    strcat(path, modulo);
    
    t_config* nuevo_config = config_create(path);
    
	free(path);
    
	if (nuevo_config == NULL) {
        log_error(logger, "No se pudo leer el archivo de configuración.");
        abort();
    }

	return nuevo_config;
}

// ------------------------------------------------------------------------------------------
// --------------------------FUNCIONES GLOBALES EXTRAS---------------------------------------
// ------------------------------------------------------------------------------------------

void list_iterate_with_data(t_list* lista, void (*func)(void*, void*), void* extra) {
    for (int i = 0; i < list_size(lista); i++) {
        void* elem = list_get(lista, i);
        func(elem, extra);
    }
}

/*
Que haga pedidos de cosas para ingresar en la prueba de conexion
*/
void establecer_datos_para_prueba_conexion() {
	char respuesta[10];
	bool continua_el_while = true;
	bool realizar_prueba;

    while (continua_el_while) {
        
        printf("¿Desea realizar una prueba de conexión? (Si/No): ");
        
        if (fgets(respuesta, sizeof(respuesta), stdin) == NULL) continue;

        // Eliminar salto de línea
        respuesta[strcspn(respuesta, "\n")] = 0;

        if (strcasecmp(respuesta, "Si") == 0) {
            realizar_prueba = true;
			continua_el_while = false;
        } else if (strcasecmp(respuesta, "No") == 0) {
            realizar_prueba = false;
			continua_el_while = false;
        } else {
            printf("Respuesta inválida. Por favor, escriba 'Si' o 'No'.\n");
        }
    }
}