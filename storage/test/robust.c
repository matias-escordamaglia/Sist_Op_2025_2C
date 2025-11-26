/*
 * test_robust.c
 * Suite de pruebas integral para el Módulo Storage.
 * Verifica: Ciclo de vida, Copy-on-Write, Persistencia, Commit y Errores.
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
#define PUERTO_STORAGE "9002"  // ¡Asegurate que coincida con storage.config!
#define TEST_WORKER_ID 999
// ---------------------

// Códigos de Error del Enunciado/Enum
#define OK 0
#define ERR_INEXISTENTE -1
#define ERR_PREEXISTENTE -2
#define ERR_ESPACIO -3
#define ERR_NO_PERMITIDO -4

// Colores para output
#define GREEN "\x1b[32m"
#define RED "\x1b[31m"
#define BLUE "\x1b[34m"
#define RESET "\x1b[0m"

typedef enum { HANDSHAKE_OK, HANDSHAKE_FALLO } t_estado_handshake;
typedef enum { CREATE, TRUNCATE, WRITE, READ, TAG, COMMIT, FLUSH, DELETE, END } Operation;
#define PAQUETE 1

// --- Serialización Básica ---
typedef struct { int size; void* stream; } t_buffer;
typedef struct { int codigo_operacion; t_buffer* buffer; } t_paquete;

t_paquete* crear_paquete(int cod_op) {
    t_paquete* p = malloc(sizeof(t_paquete));
    p->codigo_operacion = cod_op;
    p->buffer = malloc(sizeof(t_buffer));
    p->buffer->size = 0; p->buffer->stream = NULL;
    return p;
}

void liberar_paquete(t_paquete* p) {
    if(p->buffer->stream) free(p->buffer->stream);
    free(p->buffer); free(p);
}

void add(t_paquete* p, void* val, int size) {
    p->buffer->stream = realloc(p->buffer->stream, p->buffer->size + size);
    memcpy(p->buffer->stream + p->buffer->size, val, size);
    p->buffer->size += size;
}

void add_int(t_paquete* p, int val) { add(p, &val, sizeof(int)); }
void add_str(t_paquete* p, char* str) { int l = strlen(str)+1; add_int(p, l); add(p, str, l); }
void add_bin(t_paquete* p, void* d, int s) { add_int(p, s); add(p, d, s); }

void* serializar(t_paquete* p, int* bytes) {
    *bytes = p->buffer->size + 2*sizeof(int);
    void* m = malloc(*bytes); int o=0;
    memcpy(m+o, &p->codigo_operacion, 4); o+=4;
    memcpy(m+o, &p->buffer->size, 4); o+=4;
    memcpy(m+o, p->buffer->stream, p->buffer->size);
    return m;
}

// --- Red ---
int conectar() {
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(atoi(PUERTO_STORAGE));
    inet_pton(AF_INET, IP_STORAGE, &addr.sin_addr);
    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    return fd;
}

int handshake(int fd) {
    uint32_t p1=1, id=TEST_WORKER_ID; t_estado_handshake r; int bs;
    send(fd, &p1, 4, 0); recv(fd, &r, 4, MSG_WAITALL);
    if(r!=HANDSHAKE_OK) return -1;
    send(fd, &id, 4, 0); recv(fd, &r, 4, MSG_WAITALL);
    if(r!=HANDSHAKE_OK) return -1;
    recv(fd, &bs, 4, MSG_WAITALL);
    return bs;
}

// --- Wrappers de Operaciones ---
int rx_status(int fd) {
    int op, sz, st;
    recv(fd, &op, 4, MSG_WAITALL); recv(fd, &sz, 4, MSG_WAITALL);
    void* b = malloc(sz); recv(fd, b, sz, MSG_WAITALL);
    memcpy(&st, b, 4); free(b);
    return st;
}

int req_create(int fd, int q, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=CREATE; add(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return rx_status(fd);
}

int req_trunc(int fd, int q, char* f, char* t, int sz) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=TRUNCATE; add(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t); add_int(p, sz);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return rx_status(fd);
}

int req_write(int fd, int q, char* f, char* t, int blk, void* d, int len) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=WRITE; add(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    add_int(p, blk); add_bin(p, d, len);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return rx_status(fd);
}

typedef struct { int status; int size; char* data; } t_read_r;

// --- FUNCIÓN CORREGIDA AQUÍ ---
t_read_r req_read(int fd, int qid, char* f, char* t, int blk) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=READ; add(p, &op, 4); add_int(p, qid); add_str(p, f); add_str(p, t); add_int(p, blk);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    
    t_read_r resp = {0,0,NULL};
    int op_r, sz_r;
    
    // Recibir cabecera
    recv(fd, &op_r, 4, MSG_WAITALL); 
    recv(fd, &sz_r, 4, MSG_WAITALL);
    
    void* buf = malloc(sz_r); 
    recv(fd, buf, sz_r, MSG_WAITALL);
    
    int off = 0;
    memcpy(&resp.status, buf+off, 4); off+=4;
    
    if(resp.status == OK) {
        memcpy(&resp.size, buf+off, 4); off+=4; // Tamaño informado
        off+=4; // SALTARMOS el tamaño redundante del binario
        
        resp.data = malloc(resp.size);
        memcpy(resp.data, buf+off, resp.size);
    }
    free(buf);
    return resp;
}
// -------------------------------

int req_tag(int fd, int q, char* f, char* o, char* d) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=TAG; add(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, o); add_str(p, f); add_str(p, d);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return rx_status(fd);
}

int req_commit(int fd, int q, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=COMMIT; add(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return rx_status(fd);
}

int req_del(int fd, int q, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=DELETE; add(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return rx_status(fd);
}

// ==========================================
// TESTS
// ==========================================

void assert_test(char* desc, bool condition) {
    printf("%-60s [%s]\n", desc, condition ? GREEN "PASS" RESET : RED "FAIL" RESET);
}

int main() {
    printf(BLUE "=== INICIANDO TEST ROBUSTO DE STORAGE ===\n" RESET);
    int sock = conectar();
    if (sock < 0) { perror("Conexión"); return 1; }
    
    int block_size = handshake(sock);
    if (block_size < 0) { printf("Handshake falló\n"); return 1; }
    printf("Conectado. Block Size: %d\n", block_size);

    int r;
    // 1. Crear Archivo Base
    r = req_create(sock, 100, "TP_OS", "BASE");
    assert_test("Crear File TP_OS:BASE", r == OK);

    // 2. Expandir (debe asignar bloques)
    // Asumimos BlockSize=128. 300 bytes = 3 bloques (0, 1, 2)
    r = req_trunc(sock, 101, "TP_OS", "BASE", 300);
    assert_test("Truncar TP_OS:BASE a 300 bytes", r == OK);

    // 3. Escribir Datos en Bloque 0
    char* payload1 = "Datos Originales v1";
    r = req_write(sock, 102, "TP_OS", "BASE", 0, payload1, strlen(payload1)+1);
    assert_test("Escribir en Bloque 0 (BASE)", r == OK);

    // 4. Crear TAG (Debe usar Hard Links, no copiar)
    r = req_tag(sock, 103, "TP_OS", "BASE", "V1.0");
    assert_test("Crear TAG V1.0 desde BASE", r == OK);

    // 5. Verificar Contenido de V1.0 (Debe ser igual a BASE)
    t_read_r read_v1 = req_read(sock, 104, "TP_OS", "V1.0", 0);
    bool match = (read_v1.status == OK && strcmp(read_v1.data, payload1) == 0);
    assert_test("Verificar integridad de V1.0 (Lectura)", match);
    if(read_v1.data) free(read_v1.data);

    // 6. COPY-ON-WRITE: Escribir en V1.0
    // Esto DEBE crear un nuevo bloque físico para V1.0 y dejar BASE intacto.
    char* payload2 = "Datos Modificados v2";
    r = req_write(sock, 105, "TP_OS", "V1.0", 0, payload2, strlen(payload2)+1);
    assert_test("Escribir en Bloque 0 (V1.0) - Activar COW", r == OK);

    // 7. VERIFICACIÓN DE AISLAMIENTO (CRÍTICO)
    t_read_r read_base = req_read(sock, 106, "TP_OS", "BASE", 0);
    t_read_r read_v1_new = req_read(sock, 107, "TP_OS", "V1.0", 0);
    
    bool base_intacta = (read_base.status == OK && strcmp(read_base.data, payload1) == 0);
    bool v1_modificada = (read_v1_new.status == OK && strcmp(read_v1_new.data, payload2) == 0);

    assert_test("CHECK COW: BASE sigue teniendo data original", base_intacta);
    assert_test("CHECK COW: V1.0 tiene data modificada", v1_modificada);

    if(!base_intacta) printf("   -> BASE tiene: %s (Esperaba: %s)\n", read_base.data, payload1);
    if(!v1_modificada) printf("   -> V1.0 tiene: %s (Esperaba: %s)\n", read_v1_new.data, payload2);

    if(read_base.data) free(read_base.data);
    if(read_v1_new.data) free(read_v1_new.data);

    // 8. COMMIT (Debe calcular hashes y marcar COMMITED)
    r = req_commit(sock, 108, "TP_OS", "BASE");
    assert_test("Commit TP_OS:BASE", r == OK);

    // 9. RESTRICCIONES (Escribir en COMMITED)
    r = req_write(sock, 109, "TP_OS", "BASE", 0, "Intento Hack", 12);
    assert_test("Bloquear escritura en COMMITED (-4)", r == ERR_NO_PERMITIDO);

    // 10. DEDUPLICACIÓN (Commit de V1.0 con bloque 1 y 2 iguales a BASE)
    // Bloque 0 es distinto (por el COW), pero Bloque 1 y 2 nunca se tocaron,
    // por lo que siguen siendo hard links compartidos. El commit debería detectar
    // que ya existen y reusarlos (o simplemente confirmar).
    r = req_commit(sock, 110, "TP_OS", "V1.0");
    assert_test("Commit TP_OS:V1.0 (Deduplicación)", r == OK);

    // 11. DELETE
    r = req_del(sock, 111, "TP_OS", "V1.0");
    assert_test("Delete TP_OS:V1.0", r == OK);

    // 12. Verificar que BASE sigue vivo tras borrar V1.0
    t_read_r read_base_final = req_read(sock, 112, "TP_OS", "BASE", 0);
    assert_test("BASE sobrevive al delete de V1.0", read_base_final.status == OK);
    if(read_base_final.data) free(read_base_final.data);

    close(sock);
    printf(BLUE "\n=== FIN DEL TEST ===\n" RESET);
    return 0;
}