/**
 * test_robust.c
 * Suite de pruebas integral para el Storage.
 * Valida COW, Persistencia, Commit y Errores.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>

// --- CONFIGURACIÓN ---
#define IP_STORAGE "127.0.0.1"
#define PUERTO_STORAGE "9002" 
#define WORKER_ID 777
// ---------------------

// Códigos de Error (Sincronizados con tu enum)
#define OK 0
#define ERR_INEXISTENTE -1
#define ERR_PREEXISTENTE -2
#define ERR_ESPACIO -3
#define ERR_NO_PERMITIDO -4

// Colores para consola
#define GREEN "\x1b[32m"
#define RED "\x1b[31m"
#define YELLOW "\x1b[33m"
#define RESET "\x1b[0m"

typedef enum {
    HANDSHAKE_OK, HANDSHAKE_FALLO
} t_estado_handshake;

typedef enum {
    CREATE, TRUNCATE, WRITE, READ, TAG, COMMIT, FLUSH, DELETE, END
} Operation;

#define PAQUETE 1

// --- Estructuras de Serialización ---
typedef struct { int size; void* stream; } t_buffer;
typedef struct { int codigo_operacion; t_buffer* buffer; } t_paquete;

// ==========================================
// UTILIDADES DE RED Y SERIALIZACIÓN
// ==========================================

t_paquete* crear_paquete(int cod_op) {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = cod_op;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->stream = NULL;
    return paquete;
}

void liberar_paquete(t_paquete* paquete) {
    if(paquete->buffer->stream) free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void agregar_datos(t_paquete* p, void* val, int size) {
    p->buffer->stream = realloc(p->buffer->stream, p->buffer->size + size);
    memcpy(p->buffer->stream + p->buffer->size, val, size);
    p->buffer->size += size;
}

void add_int(t_paquete* p, int val) { agregar_datos(p, &val, sizeof(int)); }
void add_str(t_paquete* p, char* str) { int len = strlen(str)+1; add_int(p, len); agregar_datos(p, str, len); }
void add_bin(t_paquete* p, void* data, int size) { add_int(p, size); agregar_datos(p, data, size); }

void* serializar(t_paquete* p, int* bytes) {
    *bytes = p->buffer->size + 2*sizeof(int);
    void* magic = malloc(*bytes);
    int off = 0;
    memcpy(magic + off, &(p->codigo_operacion), sizeof(int)); off+=sizeof(int);
    memcpy(magic + off, &(p->buffer->size), sizeof(int)); off+=sizeof(int);
    memcpy(magic + off, p->buffer->stream, p->buffer->size);
    return magic;
}

int conectar() {
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(PUERTO_STORAGE));
    inet_pton(AF_INET, IP_STORAGE, &addr.sin_addr);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("Connect"); return -1; }
    return fd;
}

int handshake(int fd) {
    uint32_t p1 = 1, id = WORKER_ID;
    t_estado_handshake r; int bs;
    send(fd, &p1, sizeof(uint32_t), 0);
    recv(fd, &r, sizeof(t_estado_handshake), MSG_WAITALL);
    send(fd, &id, sizeof(uint32_t), 0);
    recv(fd, &r, sizeof(t_estado_handshake), MSG_WAITALL);
    recv(fd, &bs, sizeof(int), MSG_WAITALL);
    return bs; // Retorna Block Size
}

int recibir_status(int fd) {
    int op, size, status, off=0;
    recv(fd, &op, sizeof(int), MSG_WAITALL);
    recv(fd, &size, sizeof(int), MSG_WAITALL);
    void* buf = malloc(size);
    recv(fd, buf, size, MSG_WAITALL);
    memcpy(&status, buf, sizeof(int));
    free(buf);
    return status;
}

// ==========================================
// WRAPPERS DE OPERACIONES (Para limpiar los tests)
// ==========================================

int req_create(int fd, int qid, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = CREATE;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_truncate(int fd, int qid, char* f, char* t, int size) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TRUNCATE;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, t); add_int(p, size);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_write(int fd, int qid, char* f, char* t, int blk, void* data, int len) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = WRITE;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, t);
    add_int(p, blk); add_bin(p, data, len);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

// Read devuelve struct con status y data
typedef struct { int status; int size; char* data; } t_read_resp;

t_read_resp req_read(int fd, int qid, char* f, char* t, int blk) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = READ;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, t); add_int(p, blk);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);

    t_read_resp resp = {0,0,NULL};
    int op_r, size_r, off=0;
    recv(fd, &op_r, sizeof(int), MSG_WAITALL);
    recv(fd, &size_r, sizeof(int), MSG_WAITALL);
    void* buf = malloc(size_r);
    recv(fd, buf, size_r, MSG_WAITALL);
    
    memcpy(&resp.status, buf + off, sizeof(int)); off+=sizeof(int);
    if (resp.status == 0) {
        memcpy(&resp.size, buf + off, sizeof(int)); off+=sizeof(int);
        resp.data = malloc(resp.size);
        memcpy(resp.data, buf + off, resp.size);
    }
    free(buf);
    return resp;
}

int req_tag(int fd, int qid, char* f, char* orig, char* dest) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = TAG;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, orig); add_str(p, f); add_str(p, dest);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_commit(int fd, int qid, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = COMMIT;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_delete(int fd, int qid, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op = DELETE;
    add_datos(p, &op, sizeof(Operation)); add_int(p, qid); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

// Helper auxiliar por la falta de definición en la macro arriba
void add_datos(t_paquete* p, void* val, int size) { agregar_datos(p, val, size); }


// ==========================================
// SUITE DE PRUEBAS
// ==========================================

void print_res(char* test_name, bool pass) {
    printf("%-50s [%s]\n", test_name, pass ? GREEN "PASS" RESET : RED "FAIL" RESET);
}

void run_suite(int sock) {
    int r;
    printf("\n--- INICIANDO TEST ROBUSTO ---\n");

    // 1. INTEGRIDAD BÁSICA
    r = req_create(sock, 100, "ARCHIVO", "MAIN");
    print_res("Crear ARCHIVO:MAIN", r == OK);

    r = req_truncate(sock, 101, "ARCHIVO", "MAIN", 256); // 2 bloques de 128 (asumiendo config)
    print_res("Truncar MAIN a 256 bytes", r == OK);

    char* data1 = "DATA_BLOQUE_0_ORIGINAL";
    r = req_write(sock, 102, "ARCHIVO", "MAIN", 0, data1, strlen(data1)+1);
    print_res("Escribir en Bloque 0 de MAIN", r == OK);

    t_read_resp read1 = req_read(sock, 103, "ARCHIVO", "MAIN", 0);
    bool integrity = (read1.status == OK && strcmp(read1.data, data1) == 0);
    print_res("Leer y Verificar Integridad MAIN", integrity);
    if(read1.data) free(read1.data);

    // 2. COPY-ON-WRITE (COW) - La prueba de fuego
    r = req_tag(sock, 104, "ARCHIVO", "MAIN", "CLON");
    print_res("Crear TAG CLON desde MAIN", r == OK);

    char* data2 = "DATA_BLOQUE_0_MODIFICADA";
    r = req_write(sock, 105, "ARCHIVO", "CLON", 0, data2, strlen(data2)+1);
    print_res("Escribir en CLON (Debe activar COW)", r == OK);

    // Verificaciones Cruzadas
    t_read_resp read_main = req_read(sock, 106, "ARCHIVO", "MAIN", 0);
    t_read_resp read_clon = req_read(sock, 107, "ARCHIVO", "CLON", 0);

    bool iso_main = (read_main.status == OK && strcmp(read_main.data, data1) == 0);
    bool iso_clon = (read_clon.status == OK && strcmp(read_clon.data, data2) == 0);
    
    print_res("Verificar Aislamiento: MAIN intacto", iso_main);
    print_res("Verificar Aislamiento: CLON modificado", iso_clon);
    
    if(!iso_main) printf("   -> Esperado: %s, Recibido: %s\n", data1, read_main.data);
    if(!iso_clon) printf("   -> Esperado: %s, Recibido: %s\n", data2, read_clon.data);

    if(read_main.data) free(read_main.data);
    if(read_clon.data) free(read_clon.data);

    // 3. COMMIT y VALIDACIÓN DE ESCRITURA
    r = req_commit(sock, 108, "ARCHIVO", "MAIN");
    print_res("Commit de MAIN", r == OK);

    r = req_write(sock, 109, "ARCHIVO", "MAIN", 0, "INTENTO_HACK", 12);
    print_res("Bloquear escritura en COMMITED", r == ERR_NO_PERMITIDO);

    // 4. EXPANSIÓN DE ARCHIVO (Truncate Up)
    r = req_truncate(sock, 110, "ARCHIVO", "CLON", 512); // Subir a 4 bloques
    print_res("Expandir CLON a 512 bytes", r == OK);
    
    // Escribir en el nuevo bloque (índice 3)
    r = req_write(sock, 111, "ARCHIVO", "CLON", 3, "FINAL", 6);
    print_res("Escribir en nuevo bloque expandido", r == OK);

    // 5. LIMPIEZA
    r = req_delete(sock, 112, "ARCHIVO", "MAIN");
    print_res("Delete MAIN", r == OK);
    r = req_delete(sock, 113, "ARCHIVO", "CLON");
    print_res("Delete CLON", r == OK);

    // 6. VALIDAR LIMPIEZA
    t_read_resp read_del = req_read(sock, 114, "ARCHIVO", "MAIN", 0);
    print_res("Leer archivo borrado falla correctamente", read_del.status != OK);
}

int main() {
    int sock = conectar();
    if (sock < 0) return 1;
    int bs = handshake(sock);
    if (bs < 0) return 1;
    
    run_suite(sock);
    
    close(sock);
    return 0;
}