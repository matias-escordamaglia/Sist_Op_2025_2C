#include "memoria_interna.h"

void* memoria_interna;
uint32_t tam_memoria;
uint32_t tam_pagina;
uint32_t cant_marcos;
t_list* tabla_de_paginas;
t_bitarray* bitmap_marcos;
char* algoritmo_reemplazo;
uint32_t retardo_memoria;
int puntero_clock_;
// pthread_mutex_t mutex_mem = PTHREAD_MUTEX_INITIALIZER;
t_list* lista_global_tablas;
t_entrada_pagina** tabla_global_marcos;

void iniciar_memoria_interna(t_config *config) {
    lista_global_tablas = list_create();
    tam_memoria = (uint32_t)config_get_int_value(config, "TAM_MEMORIA");
    tam_pagina = block_size;
    cant_marcos = tam_memoria / tam_pagina;
    if (tam_memoria % tam_pagina != 0) {
        log_error(logger, "TAM_MEMORIA no multiplo");
        exit(1);
    }

    memoria_interna = malloc(tam_memoria);
    memset(memoria_interna, 0, tam_memoria);
    tabla_global_marcos = calloc(cant_marcos, sizeof(t_entrada_pagina*));

    size_t bitmap_bytes = (cant_marcos + 7) / 8;
    void *bitmap_data = malloc(bitmap_bytes);
    bitmap_marcos = bitarray_create_with_mode(bitmap_data, bitmap_bytes, LSB_FIRST);
    for (size_t i = 0; i < cant_marcos; i++) {
        bitarray_clean_bit(bitmap_marcos, i);
    }

    algoritmo_reemplazo = strdup(config_get_string_value(config, "ALGORITMO_REEMPLAZO"));
    retardo_memoria = (uint32_t)config_get_int_value(config, "RETARDO_MEMORIA");
    puntero_clock_ = 0;
    // Mutex init ya static, o pthread_mutex_init si dynamic

    log_info(logger, "Memoria init OK");
}

void destroy_memoria_interna(void) {
    free(memoria_interna);
    list_destroy_and_destroy_elements(lista_global_tablas, free_tabla);
    free(bitmap_marcos->bitarray);
    bitarray_destroy(bitmap_marcos);
    free(algoritmo_reemplazo);
}

void free_tabla(void *elem) {
    t_tabla_paginas *tabla = elem;
    free(tabla->file);
    free(tabla->tag);
    list_destroy_and_destroy_elements(tabla->paginas_proceso, free);
    free(tabla);
}

int memoria_read(t_read* r, void* buffer_destino, uint32_t id_query) {
    return acceder_memoria(r->file, r->tag, r->dir_base, buffer_destino, r->len, false, id_query);
}

int memoria_write(t_write* w, uint32_t id_query) {
    return acceder_memoria(w->file, w->tag,(uint32_t)w->dir_base, (void*)w->data, (uint32_t)w->len,true, id_query);
}

int acceder_memoria(char* file, char* tag,uint32_t dir_base, void *buffer, uint32_t tamanio,bool es_write, uint32_t id_query)
{
    pthread_mutex_lock(&mutex_mem);
    //chequear mutex mem
    t_tabla_paginas* tabla = obtener_o_crear_tabla(file, tag);
    if (!tabla ) {
        pthread_mutex_unlock(&mutex_mem);
        return ERROR_DESCONOCIDO;
    }
    if (!rango_valido(tabla, dir_base, tamanio)){
         pthread_mutex_unlock(&mutex_mem);
        return ERROR_FUERA_DE_LIMITE;
    }

    segmento_acceso seg;
    recorrido_iniciar(&seg, dir_base, tamanio, tam_pagina);

    while (seg.bytes_en_pagina > 0) {
        t_entrada_pagina* entrada = NULL;
        int estado = asegurar_pagina_presente(tabla, seg.pagina, id_query, &entrada);
        if (estado < 0) {
        log_error(logger, "Fallo al asegurar página %d", seg.pagina);
        pthread_mutex_unlock(&mutex_mem);
        return estado;
        }
    
    // Defensa extra:
        if (entrada == NULL) {
        log_error(logger, "Error crítico: asegurar_pagina devolvió éxito pero entrada es NULL");
        pthread_mutex_unlock(&mutex_mem);
        return ERROR_DESCONOCIDO;
        }

        aplicar_retardo_memoria(retardo_memoria);

        uint32_t df = direccion_fisica(entrada->marco_num, seg.offset_en_pagina, tam_pagina);

        if (es_write) {
            void* puntero_datos = (char*)buffer + seg.offset_en_buffer;
            escribir_en_memoria(df, puntero_datos, seg.bytes_en_pagina);
            marcar_modificada(entrada);
            // log obligatorio
            log_info(logger, "Query %u: Acción: ESCRIBIR - Dirección Física: %u - Valor: %.*s", 
                     id_query, df, (int)seg.bytes_en_pagina, (char*)puntero_datos);
        } else {

            void* origen = (char*)memoria_interna + df;
            void* destino = (char*)buffer + seg.offset_en_buffer;
            
            memcpy(destino, origen, seg.bytes_en_pagina);
            // log obligatorio
            log_info(logger, "Query %u: Acción: LEER - Dirección Física: %u - VALOR: %.*s", 
                     id_query, df, (int)seg.bytes_en_pagina, (char*)destino);
        }
        //habria que agregar que para cualquier acceso a la pagina se actualice el tiempo de ultimo uso para el LRU

        actualizar_reemplazo(entrada);
        recorrido_siguiente(&seg, tam_pagina);
    }

    pthread_mutex_unlock(&mutex_mem);
    return 0;
}

t_tabla_paginas* obtener_o_crear_tabla(char* file, char* tag) {
    t_tabla_paginas* tp = buscar_en_lista_global(file, tag);
    if (tp) return tp;

    crear_y_agregar_tabla_a_lista_global(file, tag);
    return buscar_en_lista_global(file, tag);
}

t_tabla_paginas* buscar_en_lista_global(char* file, char* tag) {
    for (int i = 0; i < list_size(lista_global_tablas); i++) {
        t_tabla_paginas* tabla_actual = list_get(lista_global_tablas, i);
        if (strcmp(tabla_actual->file, file) == 0 && 
            strcmp(tabla_actual->tag, tag) == 0) {
            return tabla_actual;
        }
    }
    return NULL;
}

int flush_file_tag_en_memoria(char* file, char* tag, uint32_t id_query) {
    pthread_mutex_lock(&mutex_mem);

    t_tabla_paginas* tabla = buscar_en_lista_global(file, tag);

    if (tabla == NULL) {
        log_info(logger,"No hay algo para hacer flush");
        pthread_mutex_unlock(&mutex_mem);
        return 0;
    }

    int cantidad_paginas = list_size(tabla->paginas_proceso);

    int estado_escritura = ERROR_OK_NO_FLUSH; 
    for (int i = 0; i < cantidad_paginas; i++) {
        t_entrada_pagina* entrada = (t_entrada_pagina*)list_get(tabla->paginas_proceso, i);
        if (entrada->presente && entrada->modificado) {
            
            estado_escritura = escribir_pagina_a_storage(entrada, id_query);
            if (estado_escritura == 0) {
                entrada->modificado = false;
            } else {
                
                log_error(logger, "Query %u: Error al hacer FLUSH de página %u", id_query, entrada->nro_pagina);
            }
        }
    }

    pthread_mutex_unlock(&mutex_mem);
    if(estado_escritura == -7){
      log_info(logger,"CCCCCCC");
    }
    return estado_escritura;
}

void flush_total(int queryid) {
    for (int i = 0; i < list_size(lista_global_tablas); i++) {
        // Obtener la tabla actual
        t_tabla_paginas* tabla_actual = list_get(lista_global_tablas, i);

        // Llamar a la función usando los campos de la tabla actual
        int ok = flush_file_tag_en_memoria(tabla_actual->file, tabla_actual->tag, queryid);

        // Opcional: Manejar el resultado de 'ok' si es necesario
        if (ok == 0) {
            log_info(logger, "TODO OK");
        }
    }
}

void crear_y_agregar_tabla_a_lista_global(char* file, char* tag)
{
    t_tabla_paginas* tabla_proceso = malloc(sizeof(t_tabla_paginas));
    tabla_proceso->paginas_proceso = list_create();
    tabla_proceso->file = strdup(file);
    tabla_proceso->tag = strdup(tag);
    tabla_proceso->tam_file = 0;
    list_add(lista_global_tablas, tabla_proceso);
    log_info(logger, "Tabla creada para %s:%s - Tamaño inicial: 0", file, tag); // Opcional, ayuda debug
}

bool rango_valido( t_tabla_paginas* tabla, uint32_t base, uint32_t tam) {
    if (!tabla) return false;

    if (tabla->tam_file == 0) {
        //log_warning(logger, "Validación Lazy: %s:%s tiene tamaño local 0. Delegando validación al Storage.", 
                //    tabla->file, tabla->tag;
        return true; 
    }
    // -------------------------------------------

    // Validación normal cuando SÍ sabemos el tamaño
    if (base >= tabla->tam_file) {
        log_error(logger, "Rango Inválido: Base (%u) >= Tam (%u)", base, tabla->tam_file);
        return false;
    }

    if ((uint64_t)base + (uint64_t)tam > (uint64_t)tabla->tam_file) {
        log_error(logger, "Rango Inválido: Overflow (%u > %u)", base + tam, tabla->tam_file);
        return false;
    }

    return true;
}

void recorrido_iniciar(segmento_acceso* seg, uint32_t base, uint32_t tam, uint32_t tam_p) {
    seg->pagina            = base / tam_p;
    seg->offset_en_pagina  = base % tam_p;
    seg->offset_en_buffer  = 0;
    seg->bytes_restantes   = tam;

    uint32_t capacidad = tam_p - seg->offset_en_pagina;
    seg->bytes_en_pagina = (seg->bytes_restantes < capacidad) ? seg->bytes_restantes : capacidad;
}

t_tabla_paginas* encontrar_tabla_de_entrada(t_entrada_pagina* entrada) {
    if (!entrada) return NULL;
    for (int i = 0; i < list_size(lista_global_tablas); i++) {
        t_tabla_paginas* tabla = (t_tabla_paginas*) list_get(lista_global_tablas, i);
        for (int j = 0; j < list_size(tabla->paginas_proceso); j++) {
            if (list_get(tabla->paginas_proceso, j) == entrada) {
                return tabla;
            }
        }
    }
    return NULL;
}

int asegurar_pagina_presente(t_tabla_paginas* tabla, uint32_t nro_pagina, uint32_t id_query,t_entrada_pagina** entrada_pagina) {
    t_entrada_pagina* e = get_entry(tabla, nro_pagina);
    if (e && e->presente){
        *entrada_pagina = e; 
        return 0;  
    } 

    // log obligatorio)
    log_info(logger, "Query %u: - Memoria Miss - File: %s - Tag: %s - Pagina: %u", id_query, tabla->file, tabla->tag, nro_pagina);
    
    t_entrada_pagina* victima = NULL;
    int marco = asignar_marco_o_reemplazar(&victima, id_query);
    if (marco < 0) return ERROR_ESPACIO_INSUFICIENTE;

    if (victima) {
    // (A) Si estaba modificada → sobreescribir
    if (victima->modificado) {
        int estado = escribir_pagina_a_storage(victima, id_query);
        if ( estado < 0) {
             devolver_marco(marco);
             return estado;
        }
    }

     t_tabla_paginas* v_tabla = encontrar_tabla_de_entrada(victima);
        char* v_file = v_tabla ? v_tabla->file : "(desconocido)";
        char* v_tag  = v_tabla ? v_tabla->tag  : "(desconocido)";


    
    // log obligatorio
    log_info(logger, "## Query %u: Se reemplaza la página %s:%s/%u por la %s:%s/%u",
                 id_query, v_file, v_tag, (unsigned)victima->nro_pagina, tabla->file, tabla->tag, (unsigned)nro_pagina);

    // log obligatorio
    log_info(logger, "Query %u: Se libera el Marco: %u perteneciente al - File: %s - Tag: %s",
                 id_query, (unsigned)victima->marco_num, v_file, v_tag);

    // Si tenés una función extra para liberar la víctima (ej: set presente=false, etc.)
    liberar_marco_de_victima(victima, id_query);
    }

    // Cargar la página desde Storage (común a ambos casos de miss), cargo pq la pegina que quiero no esta en Memoria interna.
    int estado_carga = cargar_pagina_desde_storage(tabla, nro_pagina, marco, id_query);
    if ( estado_carga< 0) {
        devolver_marco(marco);
        *entrada_pagina=NULL; 
        return estado_carga;
    }

    e = indico_entrada_presente(tabla, nro_pagina, marco, id_query);
    *entrada_pagina = e; 
    return 0;
}

int escribir_pagina_a_storage(t_entrada_pagina* victima, uint32_t id_query) {
   
    // 1. Identificar a quién pertenece la víctima
    t_tabla_paginas* tabla_owner = encontrar_tabla_de_entrada(victima);
    if (!tabla_owner) {
        log_info(logger, "Error: Intento de SWAP OUT de pagina huérfana (marco %d)", victima->marco_num);
        return ERROR_DESCONOCIDO;
    }

    // 2. Calcular dirección física real en el malloc grande
    uint32_t offset_memoria = victima->marco_num * tam_pagina;
    void* contenido_pagina = (char*)memoria_interna + offset_memoria;

    t_paquete* paquete = crear_paquete(); 
    insertar_uint32_a_paquete(paquete, WRITE);
    insertar_uint32_a_paquete(paquete, id_query); // Para logs del Storage
    insertar_string_a_paquete(paquete, tabla_owner->file);
    insertar_string_a_paquete(paquete, tabla_owner->tag);
    insertar_uint32_a_paquete(paquete, victima->nro_pagina);
    insertar_binario_a_paquete(paquete, contenido_pagina, tam_pagina); 

    // IMPORTANTE: Usar inserción binaria, NO string


    // 4. Enviar y liberar
    enviar_paquete(paquete, conexion_storage);

    int respuesta = recibir_operacion(conexion_storage, logger);
    
     if (respuesta != PAQUETE) {
        log_error(logger, "Query %u: Fallo al recibir paquete de Storage. Codigo: %d", id_query, respuesta);
        return ERROR_DESCONOCIDO;
    }
    int size; 
    int offset=0; 
    void* t_buffer = recibir_buffer(&size,conexion_storage); 
    int estado_operacion = extraer_int(t_buffer,&offset);
    free(t_buffer); 
    
    return estado_operacion;
}

int cargar_pagina_desde_storage(t_tabla_paginas* tabla, uint32_t nro_pagina, int marco_asignado, uint32_t id_query) {
    
    // 1. Armar paquete READ
    // Protocolo: OP_CODE | FILE | TAG | NRO_PAGINA | ID_QUERY
    t_paquete* paquete = crear_paquete();
    insertar_uint32_a_paquete(paquete, READ);
    insertar_uint32_a_paquete(paquete, id_query);
    insertar_string_a_paquete(paquete, tabla->file);
    insertar_string_a_paquete(paquete, tabla->tag);
    insertar_uint32_a_paquete(paquete, nro_pagina);

    enviar_paquete(paquete, conexion_storage);

    // 2. Recibir respuesta
    int op_code = recibir_operacion(conexion_storage, logger);
    
    if (op_code != PAQUETE) {
        log_error(logger, "Query %u: Fallo al recibir paquete de Storage. Codigo: %d", id_query, op_code);
        return ERROR_DESCONOCIDO;
    }

    int size_recibido = 0;
    void* stream_datos = recibir_buffer(&size_recibido, conexion_storage);

    if (stream_datos == NULL || size_recibido < sizeof(int)) {
         log_error(logger, "Query %u: Buffer recibido inválido o vacío", id_query);
         if(stream_datos) free(stream_datos);
         return ERROR_DESCONOCIDO;
    }

    int offset=0;

    int estado_operacion = extraer_int(stream_datos,&offset);
    if(estado_operacion==0){
        int tam_binario = 0; 
        char* contenido = extraer_binario_y_tamanio(stream_datos,&offset,&tam_binario);

        if (tam_binario > tam_pagina) {
        log_error(logger, "CRITICAL: Storage envió más bytes (%d) que el tamaño de página (%d)", tam_binario, tam_pagina);
        // Ajustamos para no romper memoria, aunque esto indica un error grave en Storage
        tam_binario = tam_pagina; 
        }

        log_info(logger, "Contenido recibido de storage:%s",contenido);
        log_info(logger, "Query %u: Recibidos %d bytes desde Storage", id_query, tam_binario); 

        // 4. Escribir en Memoria Interna
        uint32_t offset_memoria = marco_asignado * tam_pagina;
        // Copiamos directo al malloc global
        memcpy((char*)memoria_interna + offset_memoria, contenido, tam_binario);
        

        // Log obligatorio Memoria Add (segun enunciado pag 18)
        log_info(logger, "Query %u: Memoria Add - File: %s - Tag: %s - Pagina: %u - Marco: %u",
                id_query, tabla->file, tabla->tag, nro_pagina, marco_asignado);

        free(stream_datos);
        free(contenido);
        return 0;
    }
    
    log_error(logger, "Query %u: Storage rechazó la lectura. Código error: %d", id_query, estado_operacion);
    free(stream_datos);
    return estado_operacion; 
}

void liberar_marco_de_victima(t_entrada_pagina* victima, uint32_t id_query) {
    if (!victima) return;

    tabla_global_marcos[victima->marco_num] = NULL;
    victima->presente = false;
    victima->modificado = false;
    victima->marco_num = -1;
}

void devolver_marco(int marco) {
    bitarray_clean_bit(bitmap_marcos, marco); 
    tabla_global_marcos[marco] = NULL;
    log_info(logger, "Se devolvió el marco %d por error en carga", marco);
}

t_entrada_pagina* indico_entrada_presente(t_tabla_paginas* tabla, uint32_t nro_pagina, int marco, uint32_t id_query) {
    t_entrada_pagina* e = get_entry(tabla, nro_pagina);
    
    if (!e) {
        // Crear nueva entrada si no existe (paginación a demanda)
        e = malloc(sizeof(t_entrada_pagina));
        if (!e) {
            log_error(logger, "Error alloc entrada pagina");
            return NULL;
        }
        e->nro_pagina = nro_pagina;
        list_add(tabla->paginas_proceso, e);
    }
    
    // Actualizar campos
    e->presente = true;
    e->marco_num = (uint32_t)marco;
    e->modificado = false;  // Nueva o recargada, no modificada aún
    e->bit_uso = true;      // Recién accedida
    tabla_global_marcos[marco] = e;


    if (strcmp(algoritmo_reemplazo, "LRU") == 0) {
        e->ultimo_acceso = (uint64_t)time(NULL);  // Timestamp para LRU
    }
    // Para CLOCK-M, no hace falta más inicialización acá (el puntero clock maneja el ciclo)
    log_info(logger, "Query %u: Se asigna el Marco: %u a la Página: %u perteneciente al - File: %s - Tag: %s", 
             id_query, (uint32_t)marco, nro_pagina, tabla->file, tabla->tag);
    // Log obligatorio (página 17: "Se asigna el Marco...")
    // Asumiendo id_query se pasa desde caller, pero si no, sacalo o pasalo como param
    // log_info(logger, "Query %u: Se asigna el Marco: %u a la Página: %u perteneciente al - File: %s - Tag: %s.", id_query, (uint32_t)marco, nro_pagina, tabla->file, tabla->tag);
    // log_info(logger, "Query %u: Se asigna el Marco: %u a la Página: %u perteneciente al - File: %s - Tag: %s.", 
            //  id_query, (uint32_t)marco, nro_pagina, tabla->file, tabla->tag);
    return e;
}

int asignar_marco_o_reemplazar(t_entrada_pagina** victima, uint32_t id_query) {
    // Buscar un marco libre en el bitmap
    for (size_t i = 0; i < cant_marcos; i++) {
        if (!bitarray_test_bit(bitmap_marcos, i)) {
            bitarray_set_bit(bitmap_marcos, i);
            *victima = NULL;  // No se necesita reemplazo
            return (int)i;
        }
    }

    // No hay marcos libres: memoria llena.
    if (strcmp(algoritmo_reemplazo, "LRU") == 0) {
            *victima = reemplazar_pagina_lru();
        } else if (strcmp(algoritmo_reemplazo, "CLOCK-M") == 0) {
            *victima = reemplazar_pagina_clock();
        } else {
            // Manejo de error
            log_info(logger, "Algoritmo de reemplazo '%s' no soportado.", algoritmo_reemplazo);
            return -1;
        }

        if (*victima != NULL) {
            // Si se seleccionó una víctima, devolvemos el marco que ella ocupaba
            int marco_victima_num = (int)(*victima)->marco_num; 
            //
            return marco_victima_num;
        }

    return -1; // Fallback en caso de error
}

void aplicar_retardo_memoria(uint32_t milis) {
    usleep(milis * 1000);
}

uint32_t direccion_fisica(uint32_t marco, uint32_t offset, uint32_t tam_p) {
    return marco * tam_p + offset;
}

void escribir_en_memoria(uint32_t dir_fisica, void* src, uint32_t nbytes) {
    memcpy((uint8_t*)memoria_interna + dir_fisica, src, nbytes);
}

void marcar_modificada(t_entrada_pagina* e) {
    e->modificado = true;
}

void recorrido_siguiente(segmento_acceso* seg, uint32_t tam_pagina) {
    if (seg->bytes_en_pagina == 0) return;

    if (seg->bytes_restantes <= seg->bytes_en_pagina) {
        seg->bytes_restantes  = 0;
        seg->bytes_en_pagina  = 0;
        seg->offset_en_buffer += seg->bytes_en_pagina;
        return;
    }

    seg->bytes_restantes  -= seg->bytes_en_pagina;
    seg->offset_en_buffer += seg->bytes_en_pagina;

    seg->pagina           += 1;
    seg->offset_en_pagina  = 0;

    uint32_t capacidad = tam_pagina;
    seg->bytes_en_pagina = (seg->bytes_restantes < capacidad) ? seg->bytes_restantes : capacidad;
}


void actualizar_reemplazo(t_entrada_pagina* e) {
    if (!e) return;

    e->bit_uso = true;
    if (algoritmo_reemplazo && (strcmp(algoritmo_reemplazo, "LRU") == 0)) {
        e->ultimo_acceso = (uint64_t)time(NULL);
    }
    // Para CLOCK-M no hace falta más aca : el bit de modificado ya lo setea marcar_modificada()
    // en caso de WRITE. Para READ no se toca modificado.
}

t_entrada_pagina* get_entry(t_tabla_paginas* tabla, uint32_t nro_pagina) {
    for (int i = 0; i < list_size(tabla->paginas_proceso); i++) {
        t_entrada_pagina* entrada = list_get(tabla->paginas_proceso, i);
        if (entrada->nro_pagina == nro_pagina) {
          return entrada;
        }
    }
    return NULL;
}

// t_entrada_pagina* reemplazar_pagina_clock() {
//     // t_entrada_pagina* victima = NULL;
//     // int pasadas_completadas = 0;
//     uint32_t inicio_pasada;
//     //log por si acaso a
//     log_info(logger, "[CLOCK-M] Iniciando búsqueda de víctima desde el marco %u...", puntero_clock_);

//    for (int pasada = 0; pasada < 4; pasada++) {
//         log_info(logger, "[CLOCK-M] Iniciando Pasada %d (busca %s) desde Marco %u...", 
//                  pasada + 1, (pasada == 0 ? "Clase (0,0)" : "Clase (0,X)"), puntero_clock_);
//         inicio_pasada = puntero_clock_;
//         do {
//             t_entrada_pagina* entrada = tabla_global_marcos[puntero_clock_];

//             // 1. Si el marco está libre (NULL), simplemente avanzamos el puntero y continuamos.
//             if (entrada == NULL) {
//                 puntero_clock_ = (puntero_clock_ + 1) % cant_marcos; // Avanzar el puntero
//                 continue; 
//             }
            
//             bool u = entrada->bit_uso;
//             bool m = entrada->modificado;
            
//             if (pasada == 0) { // PASADA 1: Busca (0,0)
//                 if (!u && !m) {
//                     // Éxito P1
//                     puntero_clock_ = (puntero_clock_ + 1) % cant_marcos; // Avanzar y Retornar
//                     return entrada;
//                 } else if (u && pasada == 1){
//                     if(m==1){
//                         puntero_clock_ = (puntero_clock_ + 1) % cant_marcos; // Avanzar y Retornar
//                     return entrada;
//                     }
//                     else{
//                         entrada->bit_uso = false; // Limpiar U buscar modificado
//                     }
                    
//                 }
//             } else { // PASADA 2: Busca (0,X)
//                 if ((!u && m==1 && pasada==2) || (!u && m==1 && pasada==3)) {
//                     // Éxito P2
//                     puntero_clock_ = (puntero_clock_ + 1) % cant_marcos; // Avanzar y Retornar
//                     return entrada;
//                 }
//             }
            
//             // Si no se encontró víctima en esta iteración, el puntero AVANZA
//             puntero_clock_ = (puntero_clock_ + 1) % cant_marcos; 

//         } while (puntero_clock_ != inicio_pasada); // Repetir hasta dar la vuelta completa
//     }     
//     log_error(logger, "[CLOCK-M] ERROR: No se encontró víctima después de dos pasadas. Esto no debería ocurrir.");
//     return NULL; 
// }
t_entrada_pagina* reemplazar_pagina_clock() {
    log_info(logger, "[CLOCK-M] Iniciando selección de víctima. Puntero en marco: %d", puntero_clock_);
    int pasada = 0; 

    while (1) { 
        
        int inicio_vuelta = puntero_clock_;
        
        do {
            t_entrada_pagina* entrada = tabla_global_marcos[puntero_clock_];
            
            if (entrada == NULL) {
                puntero_clock_ = (puntero_clock_ + 1) % cant_marcos;
                continue;
            }

            bool u = entrada->bit_uso;
            bool m = entrada->modificado;

            // --- LÓGICA Clock  
            if (pasada == 0 || pasada == 2) { 
                if (!u && !m) {
                    t_entrada_pagina* victima = entrada;
                    
                    puntero_clock_ = (puntero_clock_ + 1) % cant_marcos;
                    
                    log_info(logger, "[CLOCK-M] Víctima encontrada en pasada %d. Marco: %u", pasada, victima->marco_num);
                    return victima;
                }
            } 
        
            else if (pasada == 1 || pasada == 3) {
                if (!u && m) {                    t_entrada_pagina* victima = entrada;
                    puntero_clock_ = (puntero_clock_ + 1) % cant_marcos;
                    
                    log_info(logger, "[CLOCK-M] Víctima encontrada en pasada %d (Sucia). Marco: %u", pasada, victima->marco_num);
                    return victima;
                }
                
                if (u) {
                    entrada->bit_uso = false;
                }
            }
            puntero_clock_ = (puntero_clock_ + 1) % cant_marcos;

        } while (puntero_clock_ != inicio_vuelta); // Repetir hasta dar la vuelta completa
        pasada++;
        
        if (pasada > 4) pasada = 0; 
    }
    log_error(logger, "[CLOCK-M] ERROR: No se encontró víctima después de dos pasadas. Esto no debería ocurrir.");
    return NULL; 
}

t_entrada_pagina* reemplazar_pagina_lru() {
    t_entrada_pagina* victima = NULL;
    // 1. Inicializa el tiempo más antiguo con un valor inmenso para que el primer tiempo real sea menor.
    uint64_t tiempo_mas_antiguo = ULLONG_MAX; 
    
    // Iteramos sobre TODOS los posibles marcos (de 0 hasta cant_marcos - 1)
    for (uint32_t i = 0; i < cant_marcos; i++) {
        // No necesitamos verificar el bitmap; si tabla_global_marcos[i] no es NULL, está ocupado.
        t_entrada_pagina* entrada = tabla_global_marcos[i]; 
        
        // Solo evaluamos entradas presentes (no NULL)
        if (entrada != NULL) {
            
            // Si el tiempo de esta página es MÁS PEQUEÑO que el 'tiempo_mas_antiguo' actual
            if (entrada->ultimo_acceso < tiempo_mas_antiguo) {
                
                tiempo_mas_antiguo = entrada->ultimo_acceso;
                victima = entrada;
            }
        }
    }
    if (victima == NULL) {
        log_error(logger, "[LRU] ERROR: No se encontró víctima. La memoria debería estar llena.");
    }
    return victima;
}

t_entrada_pagina* buscar_entrada_por_marco(uint32_t marco_num) {
        if (marco_num >= cant_marcos) return NULL;
        // 2. Iterar sobre todas las entradas de página dentro de esta tabla.
            t_entrada_pagina* entrada = tabla_global_marcos[marco_num];
            if (!entrada && bitarray_test_bit(bitmap_marcos, marco_num)) {
         log_error(logger, "Error de consistencia CRÍTICO: Marco %u en Bitmap, pero NULL en Tabla Global.", marco_num);
    }
    
    return entrada;
}

void actualizar_tam_memoria(char* file, char* tag, uint32_t nuevo_tamanio) {
    // Bloqueamos el mutex para proteger la lista global de tablas
    pthread_mutex_lock(&mutex_mem);

    t_tabla_paginas* tabla = buscar_en_lista_global(file, tag);

    // Si la tabla no existe, la creamos (Lazy Loading)
    if (tabla == NULL) {
        crear_y_agregar_tabla_a_lista_global(file, tag);
        tabla = buscar_en_lista_global(file, tag);
    }

    if (tabla != NULL) {
        uint32_t tam_anterior = tabla->tam_file;
        tabla->tam_file = nuevo_tamanio;
        
        log_info(logger, "Memoria Interna: Actualizado tamaño de %s:%s. (%u -> %u bytes)", 
                 file, tag, tam_anterior, nuevo_tamanio);
    } else {
        log_error(logger, "Memoria Interna: Error crítico al intentar actualizar tamaño de %s:%s", file, tag);
    }

    pthread_mutex_unlock(&mutex_mem);
}
void eliminar_tabla_memoria(char* file, char* tag, uint32_t id_query) {
    pthread_mutex_lock(&mutex_mem);
    
    t_tabla_paginas* tabla = buscar_en_lista_global(file, tag);
    
    if (tabla != NULL) {
      
        int cant_paginas = list_size(tabla->paginas_proceso);

        for (int i = 0; i < cant_paginas; i++) {
            t_entrada_pagina* entrada = (t_entrada_pagina*)list_get(tabla->paginas_proceso, i);
            
            if (entrada->presente) {
                log_info(logger, "Query %u: Se libera el Marco: %u perteneciente al - File: %s - Tag: %s",
                         id_query, entrada->marco_num, tabla->file, tabla->tag);

                tabla_global_marcos[entrada->marco_num] = NULL;
                
                bitarray_clean_bit(bitmap_marcos, entrada->marco_num);
            }
        }
                
        bool encontrado = false;
        for (int i = 0; i < list_size(lista_global_tablas); i++) {
            t_tabla_paginas* actual = (t_tabla_paginas*)list_get(lista_global_tablas, i);
            
            if (actual == tabla) {
        
                list_remove_and_destroy_element(lista_global_tablas, i, free_tabla);
                encontrado = true;
                break; // Importante salir del for al modificar la lista
            }
        }

        if (encontrado) {
            log_info(logger, "Memoria Interna: Tabla eliminada administrativamente para %s:%s", file, tag);
        }

    } else {
        log_warning(logger, "Query %u: Intento de eliminar tabla inexistente en memoria (%s:%s)", id_query, file, tag);
    }
    pthread_mutex_unlock(&mutex_mem);
}
