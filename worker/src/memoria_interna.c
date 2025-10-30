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
pthread_mutex_t mutex_mem = PTHREAD_MUTEX_INITIALIZER;
t_list* lista_global_tablas;

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

int memoria_write(t_write* w, uint32_t id_query) {
    return acceder_memoria(w->file, w->tag,(uint32_t)w->dir_base, (void*)w->data, (uint32_t)w->len,true, id_query);
}

int acceder_memoria(char* file, char* tag,uint32_t dir_base, void *buffer, uint32_t tamanio,bool es_write, uint32_t id_query)
{
    pthread_mutex_lock(&mutex_mem);

    t_tabla_paginas* tabla = obtener_o_crear_tabla(file, tag);
    if (!tabla || !rango_valido(tabla, dir_base, tamanio)) {
        pthread_mutex_unlock(&mutex_mem);
        return -1;
    }

    segmento_acceso seg;
    recorrido_iniciar(&seg, dir_base, tamanio, tam_pagina);

    while (seg.bytes_en_pagina > 0) {
        t_entrada_pagina* entrada = asegurar_pagina_presente(tabla, seg.pagina, id_query);
        if (!entrada) {
            pthread_mutex_unlock(&mutex_mem);
            return -1;
        }

        aplicar_retardo_memoria(retardo_memoria);

        uint32_t df = direccion_fisica(entrada->marco_num, seg.offset_en_pagina, tam_pagina);

        if (es_write) {
            escribir_en_memoria(df, (char*)buffer + seg.offset_en_buffer, seg.bytes_en_pagina);
            marcar_modificada(entrada);
            // log_escritura(id_query, df, (char*)buffer + seg.offset_en_buffer, (int)seg.bytes_en_pagina);
        } else {
            // reservado para READ en el futuro
        }

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
        if (strcmp(tabla_actual->file, file) == 0 && strcmp(tabla_actual->tag, tag) == 0) {
            return tabla_actual;
        }
    }
    return NULL;
}

void crear_y_agregar_tabla_a_lista_global(char* file, char* tag)
{
    t_tabla_paginas* tabla_proceso = malloc(sizeof(t_tabla_paginas));
    tabla_proceso->paginas_proceso = list_create();
    tabla_proceso->file = file;
    tabla_proceso->tag = tag;
    tabla_proceso->tam_file = 0;
    list_add(lista_global_tablas, tabla_proceso);
    log_info(logger, "Tabla creada para %s:%s - Tamaño inicial: 0", file, tag); // Opcional, ayuda debug
}

bool rango_valido( t_tabla_paginas* tabla, uint32_t base, uint32_t tam) {
    if (!tabla) return false;
    if (base > tabla->tam_file) return false;
    if (tam > tabla->tam_file - base) return false;
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

t_entrada_pagina* asegurar_pagina_presente(t_tabla_paginas* tabla, uint32_t nro_pagina, uint32_t id_query) {
    t_entrada_pagina* e = get_entry(tabla, nro_pagina);
    if (e && e->presente) {
        // Caso 1: La página ya está presente en memoria interna, no necesitamos cargar nada desde Storage
        return e;
    }

    // Caso 2: Page miss - La página no está presente
    // log_miss(id_query, tabla->file, tabla->tag, nro_pagina);

    t_entrada_pagina* victima = NULL;
    int marco = asignar_marco_o_reemplazar(&victima, id_query);
    if (marco < 0) return NULL;

    if (victima) {
        // Este bloque se ejecuta solo si se realizó un reemplazo (memoria llena)
        if (victima->modificado) {
            //if (escribir_pagina_a_storage(victima, id_query) < 0) return NULL;
        }
        // liberar_marco_de_victima(victima, id_query);
        // log_reemplazo(id_query, victima, tabla, nro_pagina);
    } else {
        // Marco libre asignado, no se necesitó reemplazo
    }

    // Cargar la página desde Storage (común a ambos casos de miss), cargo pq la pegina que quiero no esta en Memoria interna.
    // if (cargar_pagina_desde_storage(tabla, nro_pagina, marco, id_query) < 0) {
    //     devolver_marco(marco);
    //     return NULL;
    // }

    e = indico_entrada_presente(tabla, nro_pagina, marco);
    // log_add(id_query, tabla->file, tabla->tag, nro_pagina, (uint32_t)marco);
    return e;
}
t_entrada_pagina* indico_entrada_presente(t_tabla_paginas* tabla, uint32_t nro_pagina, int marco) {
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
    
    if (strcmp(algoritmo_reemplazo, "LRU") == 0) {
        e->ultimo_acceso = (uint64_t)time(NULL);  // Timestamp para LRU
    }
    // Para CLOCK-M, no hace falta más inicialización acá (el puntero clock maneja el ciclo)
    
    // Log obligatorio (página 17: "Se asigna el Marco...")
    // Asumiendo id_query se pasa desde caller, pero si no, sacalo o pasalo como param
    // log_info(logger, "Query %u: Se asigna el Marco: %u a la Página: %u perteneciente al - File: %s - Tag: %s.", id_query, (uint32_t)marco, nro_pagina, tabla->file, tabla->tag);
    
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
    if (true) { 
        //(LRU o CLOCK-M)
        return -1;
    }

    return -1;  // Fallback en caso de error
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