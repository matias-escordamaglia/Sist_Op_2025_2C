/*
 * test_stress_complete.c
 * Simulation of a real production environment.
 * Multiple Workers performing full file lifecycles simultaneously.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

// --- CONFIGURACIÓN ---
#define IP_STORAGE "127.0.0.1"
#define PUERTO_STORAGE "9002"
#define CANTIDAD_WORKERS 5      // Cantidad de hilos simultáneos
#define BLOCK_SIZE_REF 128      // Referencia para cálculos
// ---------------------

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOG_TEST(fmt, ...) do { \
    pthread_mutex_lock(&print_mutex); \
    printf("[W-%02d] " fmt "\n", id, ##__VA_ARGS__); \
    pthread_mutex_unlock(&print_mutex); \
} while(0)

#define OK 0
typedef enum { HANDSHAKE_OK, HANDSHAKE_FALLO } t_estado_handshake;
typedef enum { CREATE, TRUNCATE, WRITE, READ, TAG, COMMIT, FLUSH, DELETE, END } Operation;
#define PAQUETE 1

// --- Serialización ---
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

void agregar_datos(t_paquete* p, void* val, int size) {
    p->buffer->stream = realloc(p->buffer->stream, p->buffer->size + size);
    memcpy(p->buffer->stream + p->buffer->size, val, size);
    p->buffer->size += size;
}

void add_int(t_paquete* p, int val) { agregar_datos(p, &val, sizeof(int)); }
void add_str(t_paquete* p, char* str) { int l = strlen(str)+1; add_int(p, l); agregar_datos(p, str, l); }
void add_bin(t_paquete* p, void* d, int s) { add_int(p, s); agregar_datos(p, d, s); }

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

int handshake(int fd, int id_fake) {
    uint32_t p1=1, id=id_fake; t_estado_handshake r; int bs;
    send(fd, &p1, 4, 0); recv(fd, &r, 4, MSG_WAITALL);
    if(r!=HANDSHAKE_OK) return -1;
    send(fd, &id, 4, 0); recv(fd, &r, 4, MSG_WAITALL);
    if(r!=HANDSHAKE_OK) return -1;
    recv(fd, &bs, 4, MSG_WAITALL);
    return bs;
}

int recibir_status(int fd) {
    int op, sz, st;
    recv(fd, &op, 4, MSG_WAITALL); recv(fd, &sz, 4, MSG_WAITALL);
    void* b = malloc(sz); recv(fd, b, sz, MSG_WAITALL);
    memcpy(&st, b, 4); free(b);
    return st;
}

// --- Wrappers ---
int req_create(int fd, int q, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=CREATE; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_trunc(int fd, int q, char* f, char* t, int sz) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=TRUNCATE; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t); add_int(p, sz);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_write(int fd, int q, char* f, char* t, int blk, void* d, int len) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=WRITE; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    add_int(p, blk); add_bin(p, d, len);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

typedef struct { int status; int size; char* data; } t_read_r;
t_read_r req_read(int fd, int q, char* f, char* t, int blk) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=READ; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t); add_int(p, blk);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    
    t_read_r resp = {0,0,NULL};
    int op_r, sz_r;
    recv(fd, &op_r, 4, MSG_WAITALL); recv(fd, &sz_r, 4, MSG_WAITALL);
    void* buf = malloc(sz_r); recv(fd, buf, sz_r, MSG_WAITALL);
    int off=0;
    memcpy(&resp.status, buf+off, 4); off+=4;
    if(resp.status == OK) {
        memcpy(&resp.size, buf+off, 4); off+=4;
        off+=4; // SALTAR EL INT REDUNDANTE
        resp.data = malloc(resp.size);
        memcpy(resp.data, buf+off, resp.size);
    }
    free(buf);
    return resp;
}

int req_tag(int fd, int q, char* f, char* o, char* d) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=TAG; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, o); add_str(p, f); add_str(p, d);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_commit(int fd, int q, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=COMMIT; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

int req_del(int fd, int q, char* f, char* t) {
    t_paquete* p = crear_paquete(PAQUETE);
    Operation op=DELETE; agregar_datos(p, &op, 4); add_int(p, q); add_str(p, f); add_str(p, t);
    int b; void* s = serializar(p, &b); send(fd, s, b, 0); free(s); liberar_paquete(p);
    return recibir_status(fd);
}

// ==========================================
// CICLO DE VIDA DEL WORKER
// ==========================================

void check(int id, char* op, int res) {
    if(res != OK) {
        LOG_TEST("\x1b[31mERROR en %s (Code: %d)\x1b[0m", op, res);
        pthread_exit(NULL);
    }
}

void* worker_routine(void* arg) {
    int id = *(int*)arg; free(arg);
    char file[32]; sprintf(file, "FILE_W%02d", id);
    char* tag_orig = "MAIN";
    char* tag_snap = "BACKUP";
    int qid = id * 1000;

    // 1. Conectar
    int sock = conectar();
    if (sock < 0 || handshake(sock, id) < 0) { LOG_TEST("Conexión fallida"); return NULL; }
    
    LOG_TEST("Iniciando Ciclo...");

    // 2. CREATE
    check(id, "CREATE MAIN", req_create(sock, qid++, file, tag_orig));

    // 3. TRUNCATE UP (Expandir a 3 bloques)
    check(id, "TRUNC UP", req_trunc(sock, qid++, file, tag_orig, BLOCK_SIZE_REF * 3));

    // 4. WRITE (Bloque 0)
    char* data1 = "DATA_ORIGINAL_WORKER";
    check(id, "WRITE MAIN", req_write(sock, qid++, file, tag_orig, 0, data1, strlen(data1)+1));

    // 5. TAG (Checkpoint) -> Crea BACKUP
    check(id, "TAG BACKUP", req_tag(sock, qid++, file, tag_orig, tag_snap));

    // 6. WRITE (Modificar MAIN - Dispara COW)
    char* data2 = "DATA_MODIFICADA_WORKER";
    check(id, "WRITE COW", req_write(sock, qid++, file, tag_orig, 0, data2, strlen(data2)+1));

    // 7. READ & VERIFY (Validar Aislamiento)
    t_read_r r_main = req_read(sock, qid++, file, tag_orig, 0);
    t_read_r r_back = req_read(sock, qid++, file, tag_snap, 0);
    
    if (strcmp(r_main.data, data2) != 0) LOG_TEST("\x1b[31mERROR: MAIN no tiene data nueva\x1b[0m");
    if (strcmp(r_back.data, data1) != 0) LOG_TEST("\x1b[31mERROR: BACKUP fue modificado (Fallo COW)\x1b[0m");
    if (r_main.data) free(r_main.data);
    if (r_back.data) free(r_back.data);

    // 8. COMMIT (Persistir BACKUP)
    check(id, "COMMIT BACKUP", req_commit(sock, qid++, file, tag_snap));

    // 9. TRUNCATE DOWN (Achicar MAIN - Liberar bloques)
    check(id, "TRUNC DOWN", req_trunc(sock, qid++, file, tag_orig, BLOCK_SIZE_REF)); // Solo 1 bloque

    // 10. DELETE (Limpieza)
    check(id, "DELETE MAIN", req_del(sock, qid++, file, tag_orig));
    check(id, "DELETE BACKUP", req_del(sock, qid++, file, tag_snap));

    LOG_TEST("\x1b[32mCICLO COMPLETO EXITOSO\x1b[0m");
    close(sock);
    return NULL;
}

int main() {
    printf("=== INICIANDO STRESS TEST (%d Workers) ===\n", CANTIDAD_WORKERS);
    pthread_t th[CANTIDAD_WORKERS];

    for(int i=0; i<CANTIDAD_WORKERS; i++) {
        int* id = malloc(sizeof(int)); *id = i+1;
        pthread_create(&th[i], NULL, worker_routine, id);
        // Pequeño sleep para desfasar el inicio y generar caos aleatorio
        usleep(100000); 
    }

    for(int i=0; i<CANTIDAD_WORKERS; i++) pthread_join(th[i], NULL);
    
    printf("=== FIN DEL TEST ===\n");
    return 0;
}