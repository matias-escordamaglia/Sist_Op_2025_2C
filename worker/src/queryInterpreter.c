
#include "queryInterpreter.h"

void envioAQueryInterpreter(t_pedido_master_worker* pedido){
    size_t cant = 0;
    const char* const* vec = instrucciones_desde("querie1.txt", 4, &cant);
    if (!vec) {
        log_error(logger, "No hay instrucciones desde la 4 para %s", "querie1.txt");
        return;
    }

    ejecutarOperacion(pedido, vec, cant);
}

void ejecutarOperacion(t_pedido_master_worker* pedido, const char* const* instrucciones, size_t cantidad)
{
    if (!pedido || !instrucciones) { log_error(logger, "Argumentos nulos"); return; }

    // PC 1-based (si viene 0, arrancamos en 1)
    size_t pc = pedido->program_counter ? pedido->program_counter : 1;
    if (pc < 1) pc = 1;
    if (pc > cantidad) {
        log_info(logger, "PC=%zu ya está al final (cant=%zu). Nada que ejecutar.", pc, cantidad);
        return;
    }

    // Iteramos desde PC-1 hasta fin, avanzando sólo cuando la instrucción actual termina OK
    for (size_t i = pc - 1; i < cantidad; ++i) {
        const char* linea = instrucciones[i];
        log_info(logger, "INST %zu: %s", i + 1, linea);

        bool ok = ejecutar_linea(linea);

        if (!ok) {
            log_error(logger, "Fallo la instruccion %zu. Deteniendo.", i + 1);
            pedido->program_counter = i + 1; // PC queda apuntando a la fallida (1-based)
            return;
        }

        // Si fue END, cortamos ejecución (ya ejecutada)
        Operation op; const char* params=NULL;
        if (detectar_operacion(linea, &op, &params) && op == END) {
            pedido->program_counter = i + 1;
            log_info(logger, "END ejecutado. PC=%zu", pedido->program_counter);
            return;
        }

        // Avanza al siguiente
        pedido->program_counter = i + 2; // próximo a ejecutar en 1-based
    }

    log_info(logger, "Ejecución completa. PC final=%zu (cant=%zu)", pedido->program_counter, cantidad);
}

bool ejecutar_linea(const char* linea) {
    Operation op;
    const char* params = NULL;
    log_info(logger,"AAAAAAAAAAA");
    if (!detectar_operacion(linea, &op, &params)) {
        log_error(logger, "Operacion desconocida: %s", linea);
        return false;
    }

    switch (op) {
        // trabajar para que solo ejecute la instruccion siguiente una vez que la actual fue realizada
        // con exito
        case CREATE: {
            t_create c = {0};
            if (!parsear_create_params(params, &c)) {
                log_error(logger, "Sintaxis CREATE inválida: %s", linea);
                return false;
            }
            log_info(logger,"VA A EJECUTAR EL CREATE");
            bool ok = ejecutar_create(&c, CREATE);
            destruir_create(&c);
            return ok;
        }
        case TRUNCATE: {
            t_truncate tr = {0};
            if (!parsear_truncate_params(params, &tr)) {
                log_error(logger, "Sintaxis TRUNCATE inválida: %s", linea);
                return false;
            }
            log_info(logger,"VA A EJECUTAR EL TRUNCATE");
            bool ok = ejecutar_truncate(&tr);
            destruir_truncate(&tr);
            return ok;
        }
        case WRITE: {
            // t_write w = {0};
            // if (!parsear_write_params(params, &w)) {
            //     log_error(logger, "Sintaxis WRITE inválida: %s", linea);
            //     return false;
            // }
            // log_info(logger, "VA A EJECUTAR EL WRITE");

            // // Llama directo a memoria (asume query_id en pedido, ajusta si no)
            // int ok = memoria_write(&w); 
            // if (ok < 0) {
            //     log_error(logger, "[WORKER] WRITE falló para %s:%s base=%u", w.nombre_archivo, w.tag, w.dir_base);
            //     destruir_write(&w);
            //     return false;
            // }

            // // log_info(logger, "## Query %u: - Instrucción realizada: WRITE", pedido->query_id); // Log obligatorio sin params
            // destruir_write(&w);
            log_warning(logger, "READ aún no implementado: %s", linea);
            return true;
        }
        case READ: {
            // TODO: parsear y ejecutar READ <file>:<tag> <offset|bloque> <tamanio>
            log_warning(logger, "READ aún no implementado: %s", linea);
            return false;
        }
        case TAG: {
            t_tag t = {0};
            if (!parsear_tag_params(params, &t)) {
                log_error(logger, "Sintaxis TAG inválida: %s", linea);
                return false;
            }
            bool ok = ejecutar_tag(&t);
            destruir_tag(&t);
            return ok;
        }
        case COMMIT: {
            t_create c = {0};
            if (!parsear_create_params(params, &c)) {
                log_error(logger, "Sintaxis CREATE inválida: %s", linea);
                return false;
            }
            bool ok = ejecutar_create(&c, COMMIT);
            destruir_create(&c);
            return ok;
        }
        case FLUSH: {
            // TODO: parsear/ejecutar FLUSH <file>:<tag>
            log_warning(logger, "FLUSH aún no implementado: %s", linea);
            return false;
        }
        case DELETE: {
            t_create c = {0};
            if (!parsear_create_params(params, &c)) {
                log_error(logger, "Sintaxis CREATE inválida: %s", linea);
                return false;
            }
            bool ok = ejecutar_create(&c, DELETE);
            destruir_create(&c);
            return ok;
        }
        case END: {
            //return ejecutar_end(); 
            return true;            
        }
        default:
            log_error(logger, "Operacion no soportada: %d", op);
            return false;
    }
}

bool ejecutar_end(void) {
    //flush_implicito_de_query(); --> tengo que ejecutar flush antes del end.

    // 2) Notificar al Master que la query finalizó
    int ok = enviar_end_a_master(conexion_master);
    if (ok != 1) {
        log_error(logger, "[WORKER] END: fallo al notificar a Master");
        return false;
    }

    log_info(logger, "[WORKER] END notificado a Master");
    return true;
}

int enviar_end_a_master() {
    // Ejemplo:
    t_paquete* p = empaquetar_operacion_end();   // <- implementalo según tu protocolo
    if (!p) return -1;

    log_info(logger, "[STUB] Enviar a Master: END");
    enviar_paquete(p, conexion_master);
    // destruir_paquete(p);
    return 1;
}

int enviar_tag_a_storage(int conexion,const char* file_origen, const char* tag_origen,
                         const char* file_dest,const char* tag_dest)
{
    if (!file_origen || !tag_origen || !file_dest || !tag_dest) {
        log_error(logger, "TAG parámetros inválidos: fo=%p to=%p fd=%p td=%p",(void*)file_origen, (void*)tag_origen, (void*)file_dest, (void*)tag_dest);
        return -1;
    }

    // log_info(logger, "[STUB] Enviar a Storage: TAG %s:%s -> %s:%s", file_origen, tag_origen, file_dest, tag_dest);

    t_paquete* paquete = empaquetar_operacion_tag(file_origen, tag_origen, file_dest, tag_dest);
    enviar_paquete(paquete, conexion);
    // destruir_paquete(paquete); // si corresponde

    return 1; // simulamos éxito
}

bool ejecutar_create(const t_create* c, uint32_t Op) {
    // log_info(logger, "[WORKER] Ejecutando CREATE %s:%s", c->nombre_archivo, c->tag);
    int ok = enviar_create_a_storage(conexion_storage, c->nombre_archivo, c->tag, Op);
    if (ok != 1) {
        log_error(logger, "[WORKER] CREATE falló para %s:%s", c->nombre_archivo, c->tag);
        return false;
    }
    // log_info(logger, "[WORKER] CREATE OK %s:%s", c->nombre_archivo, c->tag);
    return true;
}

int enviar_create_a_storage(int conexion, const char* file, const char* tag, uint32_t Op){

    log_info(logger, "[STUB] Enviar a Storage: Op: %u  --> %s:%s" , Op, file, tag);
    t_paquete* paquete = empaquetar_operacion_create(file, tag, Op);

	enviar_paquete(paquete, conexion);
    
    return 1;
}



void destruir_create(t_create* c) {
    if (!c) return;
    free(c->nombre_archivo);
    free(c->tag);
    c->nombre_archivo = NULL;
    c->tag = NULL;
}

bool parsear_create_params(const char* params, t_create* out) {
    if (!params || !out) return false;
    params = saltar_blancos(params);
    if (*params=='\0') return false;

    char* tmp = strdup(params);
    if (!tmp) return false;

    // quitar espacios finales
    size_t n = strlen(tmp);
    while (n && (tmp[n-1]=='\n'||tmp[n-1]=='\r'||tmp[n-1]==' '||tmp[n-1]=='\t')) tmp[--n]='\0';

    char* colon = strchr(tmp, ':');
    if (!colon) { free(tmp); return false; }
    *colon = '\0';
    const char* f = tmp;
    const char* t = colon+1;
    if (*f=='\0' || *t=='\0') { free(tmp); return false; }

    out->op = CREATE;
    out->nombre_archivo = strdup(f);
    out->tag            = strdup(t);
    free(tmp);
    return out->nombre_archivo && out->tag;
}

bool detectar_operacion(const char* linea, Operation* out_op, const char** out_params) {
    const char* p = saltar_blancos(linea);
    if (empieza_con(p, "CREATE"))   { *out_op = CREATE;   *out_params = p + 6; return true; }
    if (empieza_con(p, "TRUNCATE")) { *out_op = TRUNCATE; *out_params = p + 8; return true; }
    if (empieza_con(p, "WRITE"))    { *out_op = WRITE;    *out_params = p + 5; return true; }
    if (empieza_con(p, "READ"))     { *out_op = READ;     *out_params = p + 4; return true; }
    if (empieza_con(p, "TAG"))      { *out_op = TAG;      *out_params = p + 3; return true; }
    if (empieza_con(p, "COMMIT"))   { *out_op = COMMIT;   *out_params = p + 6; return true; }
    if (empieza_con(p, "FLUSH"))    { *out_op = FLUSH;    *out_params = p + 5; return true; }
    if (empieza_con(p, "DELETE"))   { *out_op = DELETE;   *out_params = p + 6; return true; }
    if (empieza_con(p, "END"))      { *out_op = END;      *out_params = p + 3; return true; }
    return false;
}

bool ejecutar_tag(const t_tag* t) {
    if (!t) return false;

    log_info(logger, "[WORKER] Ejecutando TAG %s:%s -> %s:%s",t->file_origen, t->tag_origen, t->file_dest, t->tag_dest);

    int ok = enviar_tag_a_storage(conexion_storage,t->file_origen, t->tag_origen,t->file_dest,t->tag_dest);
    if (ok != 1) {
        log_error(logger, "[WORKER] TAG falló (%s:%s -> %s:%s)",t->file_origen, t->tag_origen, t->file_dest, t->tag_dest);
        return false;
    }

    log_info(logger, "[WORKER] TAG OK %s:%s -> %s:%s",t->file_origen, t->tag_origen, t->file_dest, t->tag_dest);
    return true;
}

void destruir_tag(t_tag* t) {
    if (!t) return;
    free(t->file_origen);  t->file_origen = NULL;
    free(t->tag_origen);   t->tag_origen  = NULL;
    free(t->file_dest);    t->file_dest   = NULL;
    free(t->tag_dest);     t->tag_dest    = NULL;
}


bool empieza_con(const char* s, const char* kw) {
    size_t n = strlen(kw);
    return strncmp(s, kw, n)==0 && (s[n]=='\0' || isspace((unsigned char)s[n]));
}

char* saltar_blancos(const char* p) {
    while (*p==' ' || *p=='\t') ++p;
    return p;
}

const char* instruccion_n(const char* nombre, size_t idx){
    t_programa* p = obtener_programa(nombre);
    if (!p || idx==0 || idx > p->cant) return NULL;
    return p->instrucciones[idx-1];
}

t_programa* obtener_programa(const char* nombre){
    return diccionario_programas ? dictionary_get(diccionario_programas, nombre) : NULL;
}

const char* const* instrucciones_desde(const char* nombre, size_t idx_1based, size_t* out_cant) {
    t_programa* p = obtener_programa(nombre);
    if (!out_cant) return NULL;
    *out_cant = 0;
    if (!p || idx_1based == 0 || idx_1based > p->cant) return NULL;

    size_t offset = idx_1based - 1;
    *out_cant = p->cant - offset;
    return (const char* const*)(p->instrucciones + offset);
}

bool parsear_truncate_params(const char* params, t_truncate* out) {
    if (!params || !out) return false;
    params = saltar_blancos(params);
    if (*params == '\0') return false;

    char* tmp = strdup(params);
    if (!tmp) return false;

    // trim trailing
    size_t n = strlen(tmp);
    while (n && (tmp[n-1]=='\n'||tmp[n-1]=='\r'||tmp[n-1]==' '||tmp[n-1]=='\t')) tmp[--n]='\0';
    if (!n) { free(tmp); return false; }

    // separamos "NOMBRE:TAG" de "TAM"
    char* sep = strpbrk(tmp, " \t");
    if (!sep) { free(tmp); return false; }   // debe existir el tamaño
    *sep = '\0';
    const char* nombre_tag = tmp;
    const char* tam_str = saltar_blancos(sep + 1);
    if (*tam_str == '\0') { free(tmp); return false; }

    // dentro de "NOMBRE:TAG" partimos por ':'
    char* colon = strchr((char*)nombre_tag, ':');
    if (!colon) { free(tmp); return false; }
    *colon = '\0';
    const char* f = nombre_tag;
    const char* t = colon + 1;
    if (*f=='\0' || *t=='\0') { free(tmp); return false; }

    // parsear tamaño (>=0)
    errno = 0;
    char* endp = NULL;
    unsigned long long val = strtoull(tam_str, &endp, 10);
    if (errno != 0 || endp == tam_str || *saltar_blancos(endp) != '\0') { free(tmp); return false; }

    out->op = TRUNCATE;
    out->nombre_archivo = strdup(f);
    out->tag            = strdup(t);
    out->tam            = (size_t)val;

    free(tmp);
    return out->nombre_archivo && out->tag;
}

bool ejecutar_truncate(const t_truncate* c) {
    if (!c) return false;
    log_info(logger, "[WORKER] Ejecutando TRUNCATE %s:%s -> tam=%zu", c->nombre_archivo, c->tag, c->tam);

    int ok = enviar_truncate_a_storage(conexion_storage, c->nombre_archivo, c->tag, c->tam);
    if (ok != 1) {
        log_error(logger, "[WORKER] TRUNCATE falló para %s:%s", c->nombre_archivo, c->tag);
        return false;
    }
    log_info(logger, "[WORKER] TRUNCATE OK %s:%s -> tam=%zu", c->nombre_archivo, c->tag, c->tam);
    return true;
}

void destruir_truncate(t_truncate* c) {
    if (!c) return;
    free(c->nombre_archivo);
    free(c->tag);
    c->nombre_archivo = NULL;
    c->tag = NULL;
    c->tam = 0;
}

int enviar_truncate_a_storage(int conexion, const char* file, const char* tag, size_t tam) {
    if (!file || !tag) {
        log_error(logger, "TRUNCATE con parametros invalidos: file=%p tag=%p", (void*)file, (void*)tag);
        return -1;
    }
    log_info(logger, "[STUB] Enviar a Storage: TRUNCATE %s:%s tam=%zu", file, tag, tam);

    t_paquete* paquete = empaquetar_operacion_truncate(file, tag, tam);
    enviar_paquete(paquete, conexion);
    // destruir_paquete(paquete); // si corresponde
    return 1; // simulamos éxito por ahora
}

bool parsear_tag_params(const char* params, t_tag* out) {
    if (!params || !out) return false;
    params = saltar_blancos(params);
    if (*params == '\0') return false;

    char* tmp = strdup(params);
    if (!tmp) return false;

    // trim trailing
    size_t n = strlen(tmp);
    while (n && (tmp[n-1]=='\n'||tmp[n-1]=='\r'||tmp[n-1]==' '||tmp[n-1]=='\t')) tmp[--n]='\0';
    if (!n) { free(tmp); return false; }

    // tomar primer token (origen)
    char* p = tmp;
    char* sp1 = strpbrk(p, " \t");
    if (!sp1) { free(tmp); return false; }
    *sp1 = '\0';
    char* origen = p;

    // tomar segundo token (destino)
    char* p2 = saltar_blancos(sp1 + 1);
    if (*p2 == '\0') { free(tmp); return false; }
    // p2 debería ser el último token (FD:TD). Si hubiera más, lo ignoramos/validamos:
    char* sp2 = strpbrk(p2, " \t");
    if (sp2) {
        // hay basura extra luego del destino → opcionalmente invalidar
        // *sp2 = '\0'; // o return false;
        *sp2 = '\0';
    }
    char* destino = p2;

    // split origen "FO:TO"
    char* colon1 = strchr(origen, ':');
    if (!colon1) { free(tmp); return false; }
    *colon1 = '\0';
    const char* fo = origen;
    const char* to = colon1 + 1;
    if (*fo == '\0' || *to == '\0') { free(tmp); return false; }

    // split destino "FD:TD"
    char* colon2 = strchr(destino, ':');
    if (!colon2) { free(tmp); return false; }
    *colon2 = '\0';
    const char* fd = destino;
    const char* td = colon2 + 1;
    if (*fd == '\0' || *td == '\0') { free(tmp); return false; }

    out->op = TAG;
    out->file_origen = strdup(fo);
    out->tag_origen  = strdup(to);
    out->file_dest   = strdup(fd);
    out->tag_dest    = strdup(td);

    bool ok = out->file_origen && out->tag_origen && out->file_dest && out->tag_dest;
    free(tmp);
    return ok;
}

bool parsear_write_params(const char* params, t_write* out) {
    if (!params || !out) return false;
    params = saltar_blancos(params);
    if (*params == '\0') return false;

    char* tmp = strdup(params);
    if (!tmp) return false;

    // Trim trailing whitespace
    size_t n = strlen(tmp);
    while (n && (tmp[n-1]=='\n'||tmp[n-1]=='\r'||tmp[n-1]==' '||tmp[n-1]=='\t')) tmp[--n]='\0';
    if (!n) { free(tmp); return false; }

    // Primer token: <FILE:TAG>
    char* sp1 = strpbrk(tmp, " \t");
    if (!sp1) { free(tmp); return false; }
    *sp1 = '\0';
    char* file_tag_str = tmp;

    // Segundo token: <DIR_BASE>
    char* p2 = saltar_blancos(sp1 + 1);
    if (*p2 == '\0') { free(tmp); return false; }
    char* sp2 = strpbrk(p2, " \t");
    if (!sp2) { free(tmp); return false; } // Debe haber contenido
    *sp2 = '\0';

    errno = 0;
    char* endp = NULL;
    unsigned long val = strtoul(p2, &endp, 10);
    if (errno != 0 || endp == p2 || *saltar_blancos(endp) != '\0') { free(tmp); return false; }

    // Tercer token: <CONTENIDO> (todo lo restante, permitimos espacios)
    char* contenido = saltar_blancos(sp2 + 1);
    if (*contenido == '\0') { free(tmp); return false; }

    // Split FILE:TAG
    char* colon = strchr(file_tag_str, ':');
    if (!colon) { free(tmp); return false; }
    *colon = '\0';
    const char* f = file_tag_str;
    const char* t = colon + 1;
    if (*f == '\0' || *t == '\0') { free(tmp); return false; }

    out->file = strdup(f);
    out->tag = strdup(t);
    out->dir_base = (size_t)val;
    out->len = strlen(contenido); // Bytes sin null
    out->data = malloc(out->len); // uint8_t*
    if (!out->data) { // Error malloc
        free(out->file);
        free(out->tag);
        free(tmp);
        return false;
    }
    memcpy(out->data, contenido, out->len); // Copia bytes

    bool ok = out->file && out->tag && out->data;
    free(tmp);
    return ok;
}

void destruir_write(t_write* w) {
    if (!w) return;
    free(w->file);
    free(w->tag);
    free(w->data);
    w->file = NULL;
    w->tag = NULL;
    w->data = NULL;
    w->len = 0;
}
