
#include "queryInterpreter.h"

void envioAQueryInterpreter(t_pedido_master_worker* pedido){
    size_t cant = 0;
    const char* const* vec = instrucciones_desde("querie1.txt", 4, &cant);
    if (!vec) {
        log_error(logger, "No hay instrucciones desde la 4 para %s", "querie1.txt");
        return;
    }

    //ejecutarOperacion(pedido, vec, cant); // descomentar esto para seguir con la ejecucion
    // de instrucciones
}

bool ejecutar_create(const t_create* c) {
    log_info(logger, "[WORKER] Ejecutando CREATE %s:%s", c->nombre_archivo, c->tag);
    int ok = enviar_create_a_storage(conexion_storage, c->nombre_archivo, c->tag);
    if (ok != 1) {
        log_error(logger, "[WORKER] CREATE falló para %s:%s", c->nombre_archivo, c->tag);
        return false;
    }
    log_info(logger, "[WORKER] CREATE OK %s:%s", c->nombre_archivo, c->tag);
    return true;
}

int enviar_create_a_storage(int conexion, const char* file, const char* tag){
    (void)conexion; // evitar warning si aún no usás el socket
    log_debug(logger, "[STUB] Enviar a Storage: CREATE %s:%s", file, tag);

    // TODO: serializar y enviar por 'conexion' (socket) según tu protocolo:
    // [int Operation=CREATE][uint32_t lenFile][file][uint32_t lenTag][tag]
    // luego recibir 'int resultado' (1 OK / 0 ERROR) y retornarlo.
    return 1; // simulamos éxito por ahora
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
            bool ok = ejecutar_create(&c);
            destruir_create(&c);
            return ok;
        }
        case TRUNCATE: {
            // TODO: parsear y ejecutar TRUNCATE <file>:<tag> <tamanio>
            log_warning(logger, "TRUNCATE aún no implementado: %s", linea);
            return false;
        }
        case WRITE: {
            // TODO: parsear y ejecutar WRITE <file>:<tag> <offset|bloque> <datos>
            log_warning(logger, "WRITE aún no implementado: %s", linea);
            return false;
        }
        case READ: {
            // TODO: parsear y ejecutar READ <file>:<tag> <offset|bloque> <tamanio>
            log_warning(logger, "READ aún no implementado: %s", linea);
            return false;
        }
        case TAG: {
            // TODO: parsear y ejecutar TAG <file:tag_origen> <file:tag_destino>
            log_warning(logger, "TAG aún no implementado: %s", linea);
            return false;
        }
        case COMMIT: {
            // TODO: parsear/ejecutar COMMIT <file>:<tag>
            log_warning(logger, "COMMIT aún no implementado: %s", linea);
            return false;
        }
        case FLUSH: {
            // TODO: parsear/ejecutar FLUSH <file>:<tag>
            log_warning(logger, "FLUSH aún no implementado: %s", linea);
            return false;
        }
        case DELETE: {
            // TODO: parsear/ejecutar DELETE <file>:<tag>
            log_warning(logger, "DELETE aún no implementado: %s", linea);
            return false;
        }
        case END: {
            log_info(logger, "[WORKER] END recibido");
            return true; // END ejecuta OK (y cortarás el bucle arriba)
        }
        default:
            log_error(logger, "Operacion no soportada: %d", op);
            return false;
    }
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
