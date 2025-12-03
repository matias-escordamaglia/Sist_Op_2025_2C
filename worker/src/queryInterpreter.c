
#include "queryInterpreter.h"

void envioAQueryInterpreter(){
    size_t cant = 0;
    // aca iba lo de pedido, osea pedido->program_counter y pedido->query_path
    // char* const* vec = instrucciones_desde("querie1.txt", 4, &cant);
    // printf("DEBUG CHECK: Puntero: %p, Contenido: '%s'\n", 
    //        (void*)query_actual.query_path, 
    //        query_actual.query_path);
    log_info(logger, "[DEBUG] Buscando instrucciones -> Path: %s | PC: %u", 
             (query_actual.query_path != NULL ? query_actual.query_path : "NULO"), 
             query_actual.pc_actual);

    char* const* vec = instrucciones_desde(query_actual.query_path, query_actual.pc_actual, &cant);     
    if (vec != NULL) {
        log_info(logger, "[DEBUG] Recuperadas %zu instrucciones (Desde PC: %u):", cant, query_actual.pc_actual);
        
        for (size_t i = 0; i < cant; i++) {
            // vec[i] es el string de la instrucción
            log_info(logger, "   -> Instr[%zu]: '%s'", i, vec[i]);
        }
    } else {
        log_error(logger, "[DEBUG] VEC es NULL. Revisa si el PC (%u) es mayor que la cantidad de lineas del archivo.", query_actual.pc_actual);
    }
    
    // char* const* vec = instrucciones_desde("querie1.txt", 4, &cant);
    if (!vec) {
        log_error(logger, "No hay instrucciones desde la 4 para %s", "querie1.txt");
        return;
    }

    ejecutarOperacion(vec, cant);
}


/*
Motivo de este hilo es el de desacoplar la ejecución de las queries de la escucha de master y no bloquearla;
De esta forma podrá escuchar los pedidos de interrupción en tiempo real
*/
void* main_lanzamiento_ejecucion(void* _){

    while(true) {

        sem_wait(sem_ejecucion_pendiente);

        envioAQueryInterpreter();


    }
    
}


void ejecutarOperacion(char* const* instrucciones, size_t cantidad)

{
    if (!instrucciones) { log_error(logger, "Argumentos nulos"); return; }

    // PC 1-based (si viene 0, arrancamos en 1)
    size_t pc = query_actual.pc_actual ? query_actual.pc_actual : 1;
    if (pc < 1) pc = 1;
    if (pc > cantidad) {
        log_info(logger, "PC=%zu ya está al final (cant=%zu). Nada que ejecutar.", pc, cantidad);
        return;
    }

    // Iteramos desde PC-1 hasta fin, avanzando sólo cuando la instrucción actual termina OK
    for (size_t i = pc - 1; i < cantidad; ++i) {
        char* linea = instrucciones[i];
        log_info(logger, "INST %zu: %s", i + 1, linea);


        bool ok = ejecutar_linea(linea, query_actual.qid_actual);


        if (!ok) {
            log_error(logger, "Fallo la instruccion %zu. Deteniendo.", i + 1);
            query_actual.pc_actual = i + 1; // PC queda apuntando a la fallida (1-based)

        
            char* texto_mockeado = strdup("Error de mockeo");
            

            deterner_ejecucion_query_error(texto_mockeado);
            return;
        }
        
        // Si fue END, cortamos ejecución (ya ejecutada)
        Operation op;
        char* params=NULL;
        if (detectar_operacion(linea, &op, &params) && op == END) {
            query_actual.pc_actual = i + 1;
            log_info(logger, "END ejecutado. PC=%d", query_actual.pc_actual);
            
            if (hay_pedido_desalojo) {
                
                hay_pedido_desalojo = false; 
                
                sem_post(sem_desalojo_pendiente); 
                log_info(logger, "Desalojo ignorado por finalización natural (END).");
            }

            deterner_ejecucion_query_finalizado();
            return;
        }

        query_actual.pc_actual = i + 2; 


        if(hay_pedido_desalojo) {
            log_info(logger, "Deteniendo ejecución por pedido de desalojo...");
            
            sem_post(sem_desalojo_pendiente);
            return;
        }
    }


    log_info(logger, "Ejecución completa. PC final=%d (cant=%ld)", query_actual.pc_actual, cantidad);

}

bool ejecutar_linea(char* linea, uint32_t queryid) {
    Operation op;
    char* params = NULL;
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

            int ok = ejecutar_create(&c,queryid);
            // en caso de retornar  1 => son parametros invalidos
            // en caso de retornar 2 => fallo la recepcion de la respuesta de storage.
            if(ok != ERROR_OK){
              finalizar_query_con_error(ERROR_QUERY, ok);
              destruir_create(&c);
              return false;
            }

            // log obligatorio
            log_info(logger, "## Query %u: - Instrucción realizada: CREATE", queryid);

            destruir_create(&c);

            return true;
        }
        case TRUNCATE: {
            t_truncate tr = {0};
            if (!parsear_truncate_params(params, &tr)) {
                log_error(logger, "Sintaxis TRUNCATE inválida: %s", linea);
                return false;
            }

            int ok = ejecutar_truncate(&tr, queryid);
             if(ok != ERROR_OK){
              finalizar_query_con_error(ERROR_QUERY, ok);
              destruir_truncate(&tr);
              return false;
            }
            actualizar_tam_memoria(tr.nombre_archivo, tr.tag, (uint32_t)tr.tam);
            // log obligatorio
            log_info(logger, "## Query %u: - Instrucción realizada: TRUNCATE", queryid);
            destruir_truncate(&tr);
            return true;
        }
        case WRITE: {
            t_write w = {0};
            if (!parsear_write_params(params, &w)) {
                log_error(logger, "Sintaxis WRITE inválida: %s", linea);
                return false;
            }

            // Llama directo a memoria (asume query_id en pedido, ajusta si no)
            int ok = memoria_write(&w, queryid);
            if (ok < 0) {
              finalizar_query_con_error(ERROR_QUERY, ok);
              destruir_write(&w);
              return false;
            }

            // log_info(logger, "## Query %u: - Instrucción realizada: WRITE", pedido->query_id); // Log obligatorio sin params
            destruir_write(&w);
            return true;
        }
        case READ: {
            t_write r = {0};
            if (!parsear_write_params(params, &r)) {
                log_error(logger, "Sintaxis READ inválida: %s", linea);
                return false;
            }

            // 1. Preparamos un buffer para recibir los datos leídos desde memoria
            void* buffer_leido = malloc(r.len);
            if (!buffer_leido) {
                log_error(logger, "Fallo malloc en READ");
                destruir_write(&r);
                return false;
            }
            // Inicializo en 0 por seguridad
            memset(buffer_leido, 0, r.len); 

            // 2. Llamada a memoria (igual que write, pero pasando el buffer vacio para llenar)
            int ok = memoria_read(&r, buffer_leido, queryid);
            
            if (ok < 0) {
                finalizar_query_con_error(ERROR_QUERY, ok);
                free(buffer_leido);
                destruir_write(&r);
                return false;
            }

            enviar_lectura_a_master(r.file, r.tag, buffer_leido, r.len, queryid);
            // Log obligatorio
            log_info(logger, "## Query %u: - Instrucción realizada: READ", queryid);

            // Limpieza
            free(buffer_leido);
            destruir_write(&r);
            return true;
        }
        case TAG: {
            t_tag t = {0};
            if (!parsear_tag_params(params, &t)) {
                log_error(logger, "Sintaxis TAG inválida: %s", linea);
                return false;
            }
            int ok = ejecutar_tag(&t,queryid);
             if(ok != ERROR_OK){
              finalizar_query_con_error(ERROR_QUERY, ok);
              destruir_tag(&t);
              return false;
            }
            // log obligatorio
            log_info(logger, "## Query %u: - Instrucción realizada: TAG", queryid);
            destruir_tag(&t);
            return true;
        }
        case COMMIT: {
            //aplicar FLUSH
            t_create c = {0};
            flush_file_tag_en_memoria(c.nombre_archivo, c.tag, queryid);
            if (!parsear_create_params(params, &c)) {
                log_error(logger, "Sintaxis COMMIT inválida: %s", linea);
                return false;
            }
            int code = ejecutar_commit(&c,queryid);
            if (code != ERROR_OK) {
                finalizar_query_con_error(ERROR_QUERY, code);
                destruir_create(&c);
                return false;
            }

            // log obligatorio (sin parámetros)
            log_info(logger, "## Query %u: - Instrucción realizada: COMMIT", queryid);

            destruir_create(&c);
            return true;
        }
        case FLUSH: {
            t_create c = {0}; 
            if (!parsear_create_params(params, &c)) {
                log_error(logger, "Sintaxis FLUSH inválida: %s", linea);
                return false;
            }

            flush_file_tag_en_memoria(c.nombre_archivo, c.tag, queryid);
            
            log_info(logger, "## Query %u: - Instrucción realizada: FLUSH", queryid);

            // 4. Limpieza
            destruir_create(&c);
            return true;
        }
        case DELETE: {
            t_create c = {0};
            flush_file_tag_en_memoria(c.nombre_archivo, c.tag, queryid);
            if (!parsear_create_params(params, &c)) {
                log_error(logger, "Sintaxis DELETE inválida: %s", linea);
                return false;
            }
            c.op = DELETE;

            int code = ejecutar_delete(&c,queryid);     // 1 = OK, ≠1 = enum/código de error
            if (code != ERROR_OK) {
                finalizar_query_con_error(ERROR_QUERY, code);
                destruir_create(&c);
                return false;
            }

            // log obligatorio (SIN parámetros)
            log_info(logger, "## Query %u: - Instrucción realizada: DELETE", queryid);

            destruir_create(&c);
            return true;
        }
        case END: {
            t_paquete* p = empaquetar_operacion_end(queryid);

            if (!p) {
                log_error(logger, "[WORKER] No pude empaquetar END (q=%u)", queryid);
                return -1;
            }

            enviar_paquete(p, conexion_master);
            // log obligatorio (SIN parámetros)
            log_info(logger, "## Query %u: - Instrucción realizada: END", queryid);

            return true;  // el for externo ya corta al detectar END
        }

        default:
            log_error(logger, "Operacion no soportada: %d", op);
            return false;
    }
}
void flush_file_tag_en_memoria(char* file, char* tag, uint32_t id_query) {
    pthread_mutex_lock(&mutex_mem);

    t_tabla_paginas* tabla = buscar_en_lista_global(file, tag);

    if (tabla == NULL) {
        pthread_mutex_unlock(&mutex_mem);
        return;
    }

    int cantidad_paginas = list_size(tabla->paginas_proceso);

    for (int i = 0; i < cantidad_paginas; i++) {
        t_entrada_pagina* entrada = (t_entrada_pagina*)list_get(tabla->paginas_proceso, i);

        if (entrada->presente && entrada->modificado) {
            if (escribir_pagina_a_storage(entrada, id_query) == 0) {
                entrada->modificado = false;
            } else {
                log_error(logger, "Query %u: Error al hacer FLUSH de página %u", id_query, entrada->nro_pagina);
            }
        }
    }

    pthread_mutex_unlock(&mutex_mem);
}

void enviar_lectura_a_master(char* file, char* tag, void* contenido, uint32_t tamanio, uint32_t query_id) {
    t_paquete* paquete = crear_paquete();
    
    // Opción recomendada (Protocolo custom):
    insertar_uint32_a_paquete(paquete, NUEVA_LECTURA);
    insertar_uint32_a_paquete(paquete, query_id);
    insertar_string_a_paquete(paquete, file);
    insertar_string_a_paquete(paquete, tag);
    
    insertar_bytes_a_paquete(paquete, contenido, tamanio);

    enviar_paquete(paquete, conexion_master);
    eliminar_paquete(paquete);
}


void finalizar_query_con_error(t_tipo_aviso_worker_master tipodeerror, int motivo) {
    
    char* error_code = storage_error_to_string(motivo);
    t_paquete* paquete = empaquetar_operacion_fin_error(tipodeerror, error_code);
    if (!paquete) {
        return;
    }
    // 2) Enviar a Master
    enviar_paquete(paquete, conexion_master);
    // 24/11 en caso de una falla en una query, se desconecta de master, pero no corta la consola.
    // 24/11 
}

char* storage_error_to_string(int motivo) {
    switch (motivo) {
        case ERROR_OK:                     return "ERROR_OK";
        case ERROR_FILE_TAG_INEXISTENTE:   return "ERROR_FILE_TAG_INEXISTENTE";
        case ERROR_FILE_TAG_PREEXISTENTE:  return "ERROR_FILE_TAG_PREEXISTENTE";
        case ERROR_ESPACIO_INSUFICIENTE:   return "ERROR_ESPACIO_INSUFICIENTE";
        case ERROR_ESCRITURA_NO_PERMITIDA: return "ERROR_ESCRITURA_NO_PERMITIDA";
        case ERROR_FUERA_DE_LIMITE:        return "ERROR_FUERA_DE_LIMITE";
        case ERROR_DESCONOCIDO:            return "ERROR_DESCONOCIDO";
        default:                           return "ERROR_DESCONOCIDO";
    }
}
///////////// EJECUCION DE INSTRUCCIONES ///////////////////////7

int ejecutar_create(t_create* c, uint32_t query_id) {

    if (!c || !c->nombre_archivo || !c->tag) {
        log_error(logger, "[WORKER] CREATE con parámetros inválidos");
        return 1; // no llego a enviarse los datos a storage
    }

    // Envío a Storage
    log_info(logger, "[STUB] Enviar a Storage: %s:%s", c->nombre_archivo, c->tag);
    t_paquete* paquete = empaquetar_operacion_create(c->nombre_archivo, c->tag, query_id);

	enviar_paquete(paquete, conexion_storage);

    int resultado = recibir_respuesta_storage(conexion_storage, logger);

    if (resultado == ERROR_OK) {
        log_info(logger, "[WORKER] Respuesta OK de Storage para CREATE %s:%s", c->nombre_archivo, c->tag);
    } else {
        // IMPORTANTE: Aca deberia finalizar la query
        log_error(logger, "[WORKER] Respuesta ERROR de Storage para CREATE %s:%s", c->nombre_archivo, c->tag);
    }
    return resultado;
}

int ejecutar_truncate(t_truncate* c,uint32_t queryid) {

     if (!c->nombre_archivo || !c->tag) {
        log_error(logger, "TRUNCATE con parametros invalidos: file=%p tag=%p", (void*)c->nombre_archivo, (void*)c->tag);
        return 1;
    }

    log_info(logger, "[STUB] Enviar a Storage: TRUNCATE %s:%s tam=%zu", c->nombre_archivo, c->tag, c->tam);

    t_paquete* paquete = empaquetar_operacion_truncate(c->nombre_archivo, c->tag, c->tam, queryid);

    enviar_paquete(paquete, conexion_storage);

    int flag = recibir_respuesta_storage(conexion_storage, logger);

    if (flag == ERROR_OK) {
        log_info(logger, "[WORKER] Respuesta OK de Storage para TRUNCATE %s:%s", c->nombre_archivo, c->tag);
    } else {
        log_error(logger, "[WORKER] Respuesta ERROR de Storage para TRUNCATE %s:%s", c->nombre_archivo, c->tag);
    }

    log_info(logger, "[WORKER] TRUNCATE OK %s:%s -> tam=%zu", c->nombre_archivo, c->tag, c->tam);
    return flag;
}

int ejecutar_tag(t_tag* t, uint32_t queryid) {
    if (!t || !t->file_origen || !t->tag_origen || !t->file_dest || !t->tag_dest) {
        log_error(logger, "TAG parámetros inválidos: fo=%p to=%p fd=%p td=%p",
                  (void*)(t ? t->file_origen : NULL),
                  (void*)(t ? t->tag_origen  : NULL),
                  (void*)(t ? t->file_dest   : NULL),
                  (void*)(t ? t->tag_dest    : NULL));
        return 1;
    }

    log_info(logger, "[STUB] Enviar a Storage: TAG %s:%s -> %s:%s", t->file_origen, t->tag_origen, t->file_dest, t->tag_dest);

    t_paquete* paquete = empaquetar_operacion_tag(t->file_origen, t->tag_origen, t->file_dest, t->tag_dest, queryid);

    enviar_paquete(paquete, conexion_storage);

    int flag = recibir_respuesta_storage(conexion_storage, logger);

    if (flag == ERROR_OK) {
        log_info(logger, "[WORKER] TAG OK %s:%s -> %s:%s", t->file_origen, t->tag_origen, t->file_dest, t->tag_dest);
    } else {
        log_error(logger, "[WORKER] Respuesta ERROR de Storage para TAG %s:%s -> %s:%s",t->file_origen, t->tag_origen, t->file_dest, t->tag_dest);
    }
    return flag; // 1=OK, ≠1=error
}

int ejecutar_commit(t_create* c, uint32_t queryid) {
    if (!c || !c->nombre_archivo || !c->tag) {
        log_error(logger, "[WORKER] COMMIT con parámetros inválidos");
        return 1;
    }

    log_info(logger, "[STUB] Enviar a Storage: COMMIT %s:%s",  c->nombre_archivo, c->tag);

    t_paquete* paquete = empaquetar_operacion_commit(c->nombre_archivo, c->tag, queryid);

    enviar_paquete(paquete, conexion_storage);

    int flag = recibir_respuesta_storage(conexion_storage, logger);

    if (flag == ERROR_OK) {
        log_info(logger, "[WORKER] COMMIT OK %s:%s", c->nombre_archivo, c->tag);
    } else {
        log_error(logger, "[WORKER] Respuesta ERROR de Storage para COMMIT %s:%s", c->nombre_archivo, c->tag);
    }
    return flag; // 1=OK, ≠1=error
}

int ejecutar_delete(t_create* c , uint32_t queryid) {
    if (!c || !c->nombre_archivo || !c->tag) {
        log_error(logger, "[WORKER] DELETE con parámetros inválidos");
        return 1;
    };

    log_info(logger, "[STUB] Enviar a Storage: DELETE %s:%s", c->nombre_archivo, c->tag);

    t_paquete* paquete = empaquetar_operacion_delete(c->nombre_archivo, c->tag, queryid);

    enviar_paquete(paquete, conexion_storage);

    int flag = recibir_respuesta_storage(conexion_storage, logger); // 1=OK, ≠1=error

    if (flag == ERROR_OK) {
        log_info(logger, "[WORKER] DELETE OK %s:%s", c->nombre_archivo, c->tag);
    } else {
        log_error(logger, "[WORKER] Respuesta ERROR de Storage para DELETE %s:%s",
                  c->nombre_archivo, c->tag);
    }
    return flag;
}



//////////////////////////////// TERMINA LA SECCION DE EJECUCION DE INSTRUCCIONES /////////////////

// int recibir_respuesta_storage(int conexion, t_log* logger) {
//     int opcode_respuesta = recibir_operacion(conexion, logger);
//     if (opcode_respuesta < 0) {
//         log_error(logger, "[WORKER] Error al recibir opcode de respuesta de Storage (conexión caída?)");
//         return 2; //fallo en la recepcion de la respuesta
//     }

//     if (opcode_respuesta != PAQUETE) {
//         log_error(logger, "[WORKER] Opcode inesperado de Storage: %d (esperaba RESPONSE=%d)", opcode_respuesta, RESPONSE);
//         return 2;
//     }

//     int size_buffer = 0;
//     void* buffer = recibir_buffer(&size_buffer, conexion);
//     if (buffer == NULL) {
//         log_error(logger, "[WORKER] Error al recibir buffer de respuesta de Storage");
//         return 2;
//     }

//     if (size_buffer < sizeof(int)) {
//         log_error(logger, "[WORKER] Buffer de respuesta inválido (demasiado chico)");
//         free(buffer);
//         return 2;
//     }

//    // 4. Deserialización: Sacamos el entero del buffer
//     int resultado_operacion;
//     memcpy(&resultado_operacion, buffer, sizeof(int));
//     free(buffer);
//     return resultado_operacion; // si salio bien la operacion => resultado_operacion = ERROR_OK(0)
// }

int recibir_respuesta_storage(int conexion, t_log* logger) {
    // MOCK ACTIVADO: Simulamos que Storage respondió OK
    
    // Simulamos que recibimos el OpCode RESPONSE (100)
    log_trace(logger, "[MOCK] Storage envió OpCode: %d (RESPONSE)", RESPONSE);

    // Simulamos que leímos el buffer y adentro venía un 0 (ERROR_OK)
    int valor_simulado_del_buffer = 0; // 0 = ÉXITO, cambialo a otro número para probar errores
    
    log_info(logger, "[MOCK] Simulando respuesta exitosa del Storage -> Retorno: %d", valor_simulado_del_buffer);

    return valor_simulado_del_buffer;
}



void destruir_create(t_create* c) {
    if (!c) return;
    free(c->nombre_archivo);
    free(c->tag);
    c->nombre_archivo = NULL;
    c->tag = NULL;
}

bool parsear_create_params( char* params, t_create* out) {
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
    char* f = tmp;
    char* t = colon+1;
    if (*f=='\0' || *t=='\0') { free(tmp); return false; }

    out->op = CREATE;
    out->nombre_archivo = strdup(f);
    out->tag            = strdup(t);
    free(tmp);
    return out->nombre_archivo && out->tag;
}

bool detectar_operacion(char* linea, Operation* out_op, char** out_params) {
    char* p = saltar_blancos(linea);
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



void destruir_tag(t_tag* t) {
    if (!t) return;
    free(t->file_origen);  t->file_origen = NULL;
    free(t->tag_origen);   t->tag_origen  = NULL;
    free(t->file_dest);    t->file_dest   = NULL;
    free(t->tag_dest);     t->tag_dest    = NULL;
}


bool empieza_con(char* s, char* kw) {
    size_t n = strlen(kw);
    return strncmp(s, kw, n)==0 && (s[n]=='\0' || isspace((unsigned char)s[n]));
}

char* saltar_blancos(char* p) {
    while (*p==' ' || *p=='\t') ++p;
    return p;
}

char* instruccion_n(char* nombre, size_t idx){
    t_programa* p = obtener_programa(nombre);
    if (!p || idx==0 || idx > p->cant) return NULL;
    return p->instrucciones[idx-1];
}

t_programa* obtener_programa(char* nombre){
    return diccionario_programas ? dictionary_get(diccionario_programas, nombre) : NULL;
}

char* const* instrucciones_desde(char* nombre, size_t idx_1based, size_t* out_cant) {
    t_programa* p = obtener_programa(nombre);
    if (!p) {
        printf("[DEBUG] Error: No se encontró el programa '%s' en el diccionario.\n", nombre);
        return NULL;
    }
    if (!out_cant) return NULL;
    *out_cant = 0;
    if (!p || idx_1based == 0 || idx_1based > p->cant) return NULL;

    size_t offset = idx_1based - 1;
    *out_cant = p->cant - offset;
    return (char* const*)(p->instrucciones + offset);
}

bool parsear_truncate_params(char* params, t_truncate* out) {
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
    char* nombre_tag = tmp;
    char* tam_str = saltar_blancos(sep + 1);
    if (*tam_str == '\0') { free(tmp); return false; }

    // dentro de "NOMBRE:TAG" partimos por ':'
    char* colon = strchr((char*)nombre_tag, ':');
    if (!colon) { free(tmp); return false; }
    *colon = '\0';
    char* f = nombre_tag;
      char* t = colon + 1;
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


void destruir_truncate(t_truncate* c) {
    if (!c) return;
    free(c->nombre_archivo);
    free(c->tag);
    c->nombre_archivo = NULL;
    c->tag = NULL;
    c->tam = 0;
}

bool parsear_tag_params(  char* params, t_tag* out) {
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
    const char* p2 = saltar_blancos(sp1 + 1);
    if (*p2 == '\0') { free(tmp); return false; }
    // p2 debería ser el último token (FD:TD). Si hubiera más, lo ignoramos/validamos:
    char* sp2 = strpbrk(p2, " \t");
    if (sp2) {
        // hay basura extra luego del destino → opcionalmente invalidar
        // *sp2 = '\0'; // o return false;
        *sp2 = '\0';
    }
    const char* destino = p2;

    // split origen "FO:TO"
    char* colon1 = strchr(origen, ':');
    if (!colon1) { free(tmp); return false; }
    *colon1 = '\0';
      char* fo = origen;
      char* to = colon1 + 1;
    if (*fo == '\0' || *to == '\0') { free(tmp); return false; }

    // split destino "FD:TD"
    char* colon2 = strchr(destino, ':');
    if (!colon2) { free(tmp); return false; }
    *colon2 = '\0';
      char* fd = destino;
      char* td = colon2 + 1;
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

bool parsear_write_params(  char* params, t_write* out) {
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
      char* f = file_tag_str;
      char* t = colon + 1;
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
