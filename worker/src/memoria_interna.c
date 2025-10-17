#include "memoria_interna.h"


t_tabla_paginas* obtener_o_crear_tp(char* file, char* tag) {
    t_tabla_paginas* tp = buscar_en_lista_global(file, tag);
    if (tp) return tp;

    tp = malloc(sizeof(*tp));
    tp->file = strdup(file);
    tp->tag  = strdup(tag);
    tp->paginas_proceso = list_create();

    list_add(lista_global_tablas, tp);
    return tp;
}

t_tabla_paginas* buscar_en_lista_global(const char* file, const char* tag) {
    if (!lista_global_tablas || !file || !tag) return NULL;

    int n = list_size(lista_global_tablas);
    for (int i = 0; i < n; i++) {
        t_tabla_paginas* tp = (t_tabla_paginas*) list_get(lista_global_tablas, i);
        if (!tp || !tp->file || !tp->tag) continue;

        if (strcmp(tp->file, file) == 0 && strcmp(tp->tag, tag) == 0) {
            return tp; // encontrada
        }
    }
    return NULL; // no está
}
