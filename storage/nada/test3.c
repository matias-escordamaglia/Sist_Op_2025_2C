/**
 * test_client.c
 * Mock Worker actualizado para soportar QueryID.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h> 

// --- CONFIGURACIÓN ---
#define IP_STORAGE "127.0.0.1"
#define PUERTO_STORAGE "9002" // ¡Asegurate que coincida con tu storage.config!
#define FAKE_WORKER_ID 66
// ---------------------

typedef enum {
    HANDSHAKE_OK,
    HANDSHAKE_FALLO
} t_estado_handshake;

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

#define PAQUETE 1

// --- Serialización Simple ---
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
    if (paquete->buffer->stream) free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int size) {
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + size);
    memcpy(paquete->buffer->stream + paquete->buffer->size, valor, size);
    paquete->buffer->size += size;
}

// Agrega un int (uint32_t) al stream
void agregar_int_a_paquete(t_paquete* paquete, int valor) {
    agregar_a_paquete(paquete, &valor, sizeof(int));
}

// Agrega string con su tamaño al frente
void agregar_string_a_paquete(t_paquete* paquete, char* string) {
    int len = strlen(string) + 1;
    agregar_a_paquete(paquete, &len, sizeof(int));
    agregar_a_paquete(paquete, string, len);
}

// Agrega binario con su tamaño al frente
void agregar_binario_a_paquete(t_paquete* paquete, void* data, int size) {
    agregar_a_paquete(paquete, &size, sizeof(int));
    agregar_a_paquete(paquete, data, size);
}

void* serializar_paquete(t_paquete* paquete, int* bytes) {
    *bytes = paquete->buffer->size + sizeof(int) + sizeof(int);
    void* magic = malloc(*bytes);
    int offset = 0;
    memcpy(magic + offset, &(paquete->codigo_operacion), sizeof(int));
    offset += sizeof(int);
    memcpy(magic + offset, &(paquete->buffer->size), sizeof(int));
    offset += sizeof(int);
    memcpy(magic + offset, paquete->buffer->stream, paquete->buffer->size);
    return magic;
}

// --- Deserialización ---
int deserializar_int(void* buffer, int* offset) {
    int valor;
    memcpy(&valor, buffer + *offset, sizeof(int));
    *offset += sizeof(int);
    return valor;
}

char* deserializar_string(void* buffer, int* offset) {
    int size = deserializar_int(buffer, offset);
    char* string = malloc(size);
    memcpy(string, buffer + *offset, size);
    *offset += size;
    return string;
}

// --- Red ---
int conectar_a_storage() {
    struct sockaddr_in serv_addr;
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) return -1;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(PUERTO_STORAGE));
    inet_pton(AF_INET, IP_STORAGE, &serv_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connect");
        return -1;
    }
    return sock_fd;
}

int realizar_handshake(int sock_fd) {
    uint32_t paso1 = 1;
    t_estado_handshake resp;
    
    send(sock_fd, &paso1, sizeof(uint32_t), 0);
    recv(sock_fd, &resp, sizeof(t_estado_handshake), MSG_WAITALL);
    
    if (resp != HANDSHAKE_OK) { printf("HS Paso 1 falló\n"); return -1; }

    uint32_t id = FAKE_WORKER_ID;
    send(sock_fd, &id, sizeof(uint32_t), 0);
    recv(sock_fd, &resp, sizeof(t_estado_handshake), MSG_WAITALL);

    if (resp != HANDSHAKE_OK) { printf("HS Paso 2 falló\n"); return -1; }

    int block_size;
    recv(sock_fd, &block_size, sizeof(int), MSG_WAITALL);
    printf(">>> Conectado a Storage. BLOCK_SIZE: %d\n", block_size);
    return 0;
}

int esperar_respuesta_simple(int sock_fd) {
    int cod_op, size, estado;
    recv(sock_fd, &cod_op, sizeof(int), MSG_WAITALL);
    recv(sock_fd, &size, sizeof(int), MSG_WAITALL);
    void* buffer = malloc(size);
    recv(sock_fd, buffer, size, MSG_WAITALL);
    
    int offset = 0;
    estado = deserializar_int(buffer, &offset);
    free(buffer);
    return estado;
}

void enviar_paquete_socket(int sock_fd, t_paquete* p) {
    int bytes;
    void* stream = serializar_paquete(p, &bytes);
    send(sock_fd, stream, bytes, 0);
    free(stream);
    liberar_paquete(p);
}

// ==========================================
// TESTS ESPECÍFICOS
// ==========================================

void test_create(int sock, int q_id, char* file, char* tag) {
    printf("\n[TEST CREATE] ID: %d | %s:%s\n", q_id, file, tag);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = CREATE;
    
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);     // <--- ¡NUEVO! QueryID
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag);

    enviar_paquete_socket(sock, p);
    
    int res = esperar_respuesta_simple(sock);
    printf("  -> Resultado: %d %s\n", res, (res == 0 ? "(OK)" : "(ERROR)"));
}

void test_truncate(int sock, int q_id, char* file, char* tag, int tamanio) {
    printf("\n[TEST TRUNCATE] ID: %d | %s:%s -> %d bytes\n", q_id, file, tag, tamanio);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TRUNCATE;
    
    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag);
    agregar_int_a_paquete(p, tamanio); // Tamaño como int/uint32

    enviar_paquete_socket(sock, p);
    
    int res = esperar_respuesta_simple(sock);
    printf("  -> Resultado: %d %s\n", res, (res == 0 ? "(OK)" : "(ERROR)"));
}

void test_write(int sock, int q_id, char* file, char* tag, int bloque, char* contenido) {
    printf("\n[TEST WRITE] ID: %d | %s:%s Bloque %d\n", q_id, file, tag, bloque);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = WRITE;
    int tam_cont = strlen(contenido) + 1;

    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag);
    agregar_int_a_paquete(p, bloque);
    agregar_binario_a_paquete(p, contenido, tam_cont);

    enviar_paquete_socket(sock, p);
    
    int res = esperar_respuesta_simple(sock);
    printf("  -> Resultado: %d %s\n", res, (res == 0 ? "(OK)" : "(ERROR)"));
}

void test_read(int sock, int q_id, char* file, char* tag, int bloque) {
    printf("\n[TEST READ] ID: %d | %s:%s Bloque %d\n", q_id, file, tag, bloque);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = READ;

    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag);
    agregar_int_a_paquete(p, bloque);

    enviar_paquete_socket(sock, p);

    // Recepción especial para READ
    int cod_op, size;
    recv(sock, &cod_op, sizeof(int), MSG_WAITALL);
    recv(sock, &size, sizeof(int), MSG_WAITALL);
    void* buffer = malloc(size);
    recv(sock, buffer, size, MSG_WAITALL);

    int offset = 0;
    int estado = deserializar_int(buffer, &offset);
    int tam_leido = deserializar_int(buffer, &offset);
    
    if (estado == 0) {
        char* contenido = malloc(tam_leido);
        // Ojo: extraer_binario_y_tamanio en servidor pone [size][data].
        // Tu enviar_paquete_read pone [size][data]? NO.
        // Tu enviar_paquete_read usa insertar_binario_a_paquete que pone [size][data].
        // PERO en tu funcion read ya leiste el tamaño antes.
        // Vamos a asumir que deserializas directo los bytes:
        int size_payload;
        memcpy(&size_payload, buffer + offset, sizeof(int)); // El size del binario
        offset += sizeof(int);
        memcpy(contenido, buffer + offset, size_payload);
        
        printf("  -> Resultado: OK. Leído (%d bytes): '%s'\n", size_payload, contenido);
        free(contenido);
    } else {
        printf("  -> Resultado: ERROR (%d)\n", estado);
    }
    free(buffer);
}

void test_tag(int sock, int q_id, char* file, char* tag_origen, char* tag_destino) {
    printf("\n[TEST TAG] ID: %d | %s:%s -> %s\n", q_id, file, tag_origen, tag_destino);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TAG;

    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag_origen);
    agregar_string_a_paquete(p, file);        // Destino file (mismo nombre)
    agregar_string_a_paquete(p, tag_destino); // Destino tag

    enviar_paquete_socket(sock, p);
    
    int res = esperar_respuesta_simple(sock);
    printf("  -> Resultado: %d %s\n", res, (res == 0 ? "(OK)" : "(ERROR)"));
}

void test_commit(int sock, int q_id, char* file, char* tag) {
    printf("\n[TEST COMMIT] ID: %d | %s:%s\n", q_id, file, tag);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = COMMIT;

    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag);

    enviar_paquete_socket(sock, p);
    
    int res = esperar_respuesta_simple(sock);
    printf("  -> Resultado: %d %s\n", res, (res == 0 ? "(OK)" : "(ERROR)"));
}

void test_delete(int sock, int q_id, char* file, char* tag) {
    printf("\n[TEST DELETE] ID: %d | %s:%s\n", q_id, file, tag);
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = DELETE;

    agregar_a_paquete(p, &op, sizeof(Operation));
    agregar_int_a_paquete(p, q_id);
    agregar_string_a_paquete(p, file);
    agregar_string_a_paquete(p, tag);

    enviar_paquete_socket(sock, p);
    
    int res = esperar_respuesta_simple(sock);
    printf("  -> Resultado: %d %s\n", res, (res == 0 ? "(OK)" : "(ERROR)"));
}

int main() {
    int sock = conectar_a_storage();
    if (sock < 0) return 1;
    
    if (realizar_handshake(sock) < 0) return 1;

    // --- SECUENCIA DE PRUEBAS ---
    
    // 1. Crear Archivo
    test_create(sock, 100, "TEST_FILE", "BASE");
    sleep(1);

    // 2. Truncar (Asignar 2 bloques aprox, si BLOCK_SIZE=128 -> 256 bytes)
    test_truncate(sock, 101, "TEST_FILE", "BASE", 256);
    sleep(1);

    // 3. Escribir en bloque 0
    test_write(sock, 102, "TEST_FILE", "BASE", 0, "Hola mundo bloque 0");
    sleep(1);

    // 4. Leer bloque 0
    test_read(sock, 103, "TEST_FILE", "BASE", 0);
    sleep(1);

    // 5. Crear un TAG (Checkpoint)
    test_tag(sock, 104, "TEST_FILE", "BASE", "V2");
    sleep(1);

    // 6. Escribir en el TAG V2 (Copy-on-Write)
    test_write(sock, 105, "TEST_FILE", "V2", 0, "Hola mundo MODIFICADO V2");
    sleep(1);

    // 7. Leer Original (Debería seguir diciendo "Hola mundo bloque 0")
    printf("\n--- Verificando aislamiento (Base) ---\n");
    test_read(sock, 106, "TEST_FILE", "BASE", 0);
    
    // 8. Leer Modificado (Debería decir "Hola mundo MODIFICADO V2")
    printf("\n--- Verificando aislamiento (V2) ---\n");
    test_read(sock, 107, "TEST_FILE", "V2", 0);
    sleep(1);

    // 9. Commit V2
    test_commit(sock, 108, "TEST_FILE", "V2");
    sleep(1);

    // 10. Delete Base
    test_delete(sock, 109, "TEST_FILE", "BASE");

    close(sock);
    return 0;
}