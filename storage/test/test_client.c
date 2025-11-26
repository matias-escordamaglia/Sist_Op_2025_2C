/**
 * test_client.c
 *
 * Mock Worker para testear el módulo Storage.
 * Este programa simula ser un Worker, se conecta al Storage,
 * realiza el handshake y envía operaciones de prueba.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h> // Para uint32_t

// --- Configuración del Test ---
#define IP_STORAGE "127.0.0.1"
#define PUERTO_STORAGE "9002" // <--- ¡CAMBIÁ ESTO POR TU PUERTO_ESCUCHA!
#define FAKE_WORKER_ID 99
// -----------------------------


// --- Definiciones que copiamos de tu proyecto ---

// Estados del Handshake
typedef enum {
    HANDSHAKE_OK,
    HANDSHAKE_FALLO
} t_estado_handshake;

// Operaciones
typedef enum {
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

// Códigos de Paquete (simplificado)
#define PAQUETE 1
// --- Fin de definiciones ---


// --- Mini-biblioteca de Paquetes (para no depender de commons) ---

typedef struct {
    int size;
    void* stream;
} t_buffer;

typedef struct {
    int codigo_operacion;
    t_buffer* buffer;
} t_paquete;

t_paquete* crear_paquete(int cod_op) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = cod_op;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->stream = NULL;
    return paquete;
}

void liberar_paquete(t_paquete* paquete) {
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int size) {
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + size);
    memcpy(paquete->buffer->stream + paquete->buffer->size, valor, size);
    paquete->buffer->size += size;
}

void agregar_string_a_paquete(t_paquete* paquete, char* string) {
    uint32_t len = strlen(string) + 1;
    agregar_a_paquete(paquete, &len, sizeof(uint32_t));
    agregar_a_paquete(paquete, string, len);
}

void* serializar_paquete(t_paquete* paquete, int* bytes) {
    *bytes = paquete->buffer->size + sizeof(int) + sizeof(int); // cod_op + size + data
    void* magic = malloc(*bytes);
    int offset = 0;
    memcpy(magic + offset, &(paquete->codigo_operacion), sizeof(int));
    offset += sizeof(int);
    memcpy(magic + offset, &(paquete->buffer->size), sizeof(int));
    offset += sizeof(int);
    memcpy(magic + offset, paquete->buffer->stream, paquete->buffer->size);
    return magic;
}

// --- Funciones de Deserialización (para la respuesta) ---
int deserializar_int(void* buffer) {
    int valor;
    memcpy(&valor, buffer, sizeof(int));
    return valor;
}

// --- Fin de Mini-biblioteca ---


/**
 * @brief Conecta al servidor Storage.
 * @return El file descriptor del socket, o -1 si falla.
 */
int conectar_a_storage() {
    struct sockaddr_in serv_addr;
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Error creando socket");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(PUERTO_STORAGE));
    if (inet_pton(AF_INET, IP_STORAGE, &serv_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(sock_fd);
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error al conectar");
        close(sock_fd);
        return -1;
    }
    printf("Conectado a Storage en %s:%s\n", IP_STORAGE, PUERTO_STORAGE);
    return sock_fd;
}

/**
 * @brief Realiza el handshake de 3 pasos con Storage.
 * @return 0 en éxito, -1 en fallo.
 */
int realizar_handshake(int sock_fd) {
    uint32_t paso1_envio = 1;
    t_estado_handshake paso1_recv;
    uint32_t paso2_envio = FAKE_WORKER_ID;
    t_estado_handshake paso2_recv;
    int paso3_recv_block_size;

    printf("Iniciando Handshake...\n");

    // Paso 1: Enviar '1' y recibir OK
    if (send(sock_fd, &paso1_envio, sizeof(uint32_t), 0) <= 0) {
        perror("HS Paso 1 (send) falló"); return -1;
    }
    if (recv(sock_fd, &paso1_recv, sizeof(t_estado_handshake), MSG_WAITALL) <= 0) {
        perror("HS Paso 1 (recv) falló"); return -1;
    }
    if (paso1_recv != HANDSHAKE_OK) {
        printf("HS Paso 1: Storage rechazó (recibido: %d)\n", paso1_recv); return -1;
    }
    printf("Handshake Paso 1 OK\n");

    // Paso 2: Enviar ID y recibir OK
    if (send(sock_fd, &paso2_envio, sizeof(uint32_t), 0) <= 0) {
        perror("HS Paso 2 (send) falló"); return -1;
    }
    if (recv(sock_fd, &paso2_recv, sizeof(t_estado_handshake), MSG_WAITALL) <= 0) {
        perror("HS Paso 2 (recv) falló"); return -1;
    }
    if (paso2_recv != HANDSHAKE_OK) {
        printf("HS Paso 2: Storage rechazó (recibido: %d)\n", paso2_recv); return -1;
    }
    printf("Handshake Paso 2 OK (ID: %d)\n", FAKE_WORKER_ID);

    // Paso 3: Recibir BLOCK_SIZE
    if (recv(sock_fd, &paso3_recv_block_size, sizeof(int), MSG_WAITALL) <= 0) {
        perror("HS Paso 3 (recv BLOCK_SIZE) falló"); return -1;
    }
    printf("Handshake Paso 3 OK (BLOCK_SIZE recibido: %d)\n", paso3_recv_block_size);
    
    printf("--- Handshake Completo ---\n");
    return 0;
}

/**
 * @brief Espera la respuesta de estado de Storage.
 * @return El estado (0 = OK, -1 = Error).
 */
int esperar_respuesta(int sock_fd) {
    int cod_op, size;
    // 1. Recibir Cod Op
    if (recv(sock_fd, &cod_op, sizeof(int), MSG_WAITALL) <= 0) {
        perror("Error recibiendo respuesta (cod_op)"); return -1;
    }
    // 2. Recibir Tamaño
    if (recv(sock_fd, &size, sizeof(int), MSG_WAITALL) <= 0) {
        perror("Error recibiendo respuesta (size)"); return -1;
    }
    // 3. Recibir Buffer
    void* buffer = malloc(size);
    if (recv(sock_fd, buffer, size, MSG_WAITALL) <= 0) {
        perror("Error recibiendo respuesta (buffer)"); free(buffer); return -1;
    }
    
    // 4. Deserializar el estado
    int estado = deserializar_int(buffer);
    free(buffer);
    return estado;
}

/**
 * @brief Envía una operación serializada.
 */
void enviar_operacion(int sock_fd, t_paquete* paquete) {
    int size;
    void* buffer_envio = serializar_paquete(paquete, &size);
    
    if (send(sock_fd, buffer_envio, size, 0) <= 0) {
        perror("Error al enviar paquete de operación");
    }
    
    free(buffer_envio);
    liberar_paquete(paquete);
}

// --- Casos de Prueba ---

void test_create_ok(int sock_fd) {
    printf("\n--- Test: CREATE OK (MATERIAS:BASE) ---\n");
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = CREATE;
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_string_a_paquete(p, "MATERIAS");
    agregar_string_a_paquete(p, "BASE");
    
    enviar_operacion(sock_fd, p);
    
    int estado = esperar_respuesta(sock_fd);
    if (estado == 0) printf("RESULTADO: OK (Estado 0)\n");
    else printf("RESULTADO: FALLÓ (Estado %d)\n", estado);
}

void test_create_fail(int sock_fd) {
    printf("\n--- Test: CREATE FAIL (MATERIAS:BASE ya existe) ---\n");
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = CREATE;
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_string_a_paquete(p, "MATERIAS");
    agregar_string_a_paquete(p, "BASE");
    
    enviar_operacion(sock_fd, p);
    
    int estado = esperar_respuesta(sock_fd);
    if (estado == -1) printf("RESULTADO: OK (Falló como se esperaba. Estado %d)\n", estado);
    else printf("RESULTADO: FALLÓ (Debería haber dado error. Estado %d)\n", estado);
}

void test_truncate_ok(int sock_fd) {
    printf("\n--- Test: TRUNCATE OK (MATERIAS:BASE a 1024) ---\n");
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TRUNCATE;
    uint32_t tamanio = 1024;
    
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_string_a_paquete(p, "MATERIAS");
    agregar_string_a_paquete(p, "BASE");
    agregar_a_paquete(p, &tamanio, sizeof(uint32_t));
    
    enviar_operacion(sock_fd, p);
    
    int estado = esperar_respuesta(sock_fd);
    if (estado == 0) printf("RESULTADO: OK (Estado 0)\n");
    else printf("RESULTADO: FALLÓ (Estado %d)\n", estado);
}

void test_commit_ok(int sock_fd) {
    printf("\n--- Test: COMMIT OK (MATERIAS:BASE) ---\n");
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = COMMIT;
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_string_a_paquete(p, "MATERIAS");
    agregar_string_a_paquete(p, "BASE");
    
    enviar_operacion(sock_fd, p);
    
    int estado = esperar_respuesta(sock_fd);
    if (estado == 0) printf("RESULTADO: OK (Estado 0)\n");
    else printf("RESULTADO: FALLÓ (Estado %d)\n", estado);
}

void test_truncate_fail_commited(int sock_fd) {
    printf("\n--- Test: TRUNCATE FAIL (MATERIAS:BASE está COMMITED) ---\n");
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TRUNCATE;
    uint32_t tamanio = 2048;
    
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_string_a_paquete(p, "MATERIAS");
    agregar_string_a_paquete(p, "BASE");
    agregar_a_paquete(p, &tamanio, sizeof(uint32_t));
    
    enviar_operacion(sock_fd, p);
    
    int estado = esperar_respuesta(sock_fd);
    if (estado == -1) printf("RESULTADO: OK (Falló como se esperaba. Estado %d)\n", estado);
    else printf("RESULTADO: FALLÓ (Debería haber dado error. Estado %d)\n", estado);
}

void test_tag_ok(int sock_fd) {
    printf("\n--- Test: TAG OK (MATERIAS:BASE -> MATERIAS:V2) ---\n");
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TAG;
    
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_string_a_paquete(p, "MATERIAS"); // Origen
    agregar_string_a_paquete(p, "BASE");
    agregar_string_a_paquete(p, "MATERIAS"); // Destino
    agregar_string_a_paquete(p, "V2");
    
    enviar_operacion(sock_fd, p);
    
    int estado = esperar_respuesta(sock_fd);
    if (estado == 0) printf("RESULTADO: OK (Estado 0)\n");
    else printf("RESULTADO: FALLÓ (Estado %d)\n", estado);
}


int main() {
    int sock_fd = conectar_a_storage();
    if (sock_fd == -1) {
        return EXIT_FAILURE;
    }

    if (realizar_handshake(sock_fd) == -1) {
        close(sock_fd);
        return EXIT_FAILURE;
    }

    // --- Ejecutamos la secuencia de pruebas ---
    test_create_ok(sock_fd);
    sleep(1);
    test_create_fail(sock_fd);
    sleep(1);
    test_truncate_ok(sock_fd);
    sleep(1);
    test_commit_ok(sock_fd);
    sleep(1);
    test_truncate_fail_commited(sock_fd);
    sleep(1);
    test_tag_ok(sock_fd);
    
    // Aquí podés agregar más pruebas para TAG (fallido), DELETE, etc.

    printf("\n--- Pruebas finalizadas ---\n");
    close(sock_fd);
    return EXIT_SUCCESS;
}