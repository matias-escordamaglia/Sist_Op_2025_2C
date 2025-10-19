#include "memoria_interna_.h"
#include <stdbool.h>
#include <stddef.h>

// Variables globales
int tam_bloque;
void* memoria_interna = NULL;
int cantidad_paginas;
int TAM_MEMORIA_TOTAL;
int RETARDO_MEMORIA;

typedef struct {
    bool en_uso;       // Si hay una página cargada
    bool bit_uso;      // Bit usado por CLOCK
    bool modificado;   // Si fue modificada 
    int nro_pagina;    // ID de la página
    void* frame;       // Dirección dentro del malloc
} t_pagina;

// Tabla de páginas
t_pagina* tabla_paginas = NULL;

// Puntero del reloj
int puntero_clock = 0;

// Mutex para concurrencia
pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;

// --------------------------------------------------

void pasar_bloque_a_memoria(int* block_size) {
    tam_bloque = *block_size;
}


void inicializar_memoria_interna() {
    t_config* config = config_create("worker.config");
    TAM_MEMORIA_TOTAL = config_get_int_value(config, "TAM_MEMORIA");
    cantidad_paginas = TAM_MEMORIA_TOTAL / tam_bloque;
    memoria_interna = malloc(TAM_MEMORIA_TOTAL);
    tabla_paginas = malloc(sizeof(t_pagina) * cantidad_paginas);

    for (int i = 0; i < cantidad_paginas; i++) {
        tabla_paginas[i].en_uso = false;
        tabla_paginas[i].bit_uso = false;
        tabla_paginas[i].modificado = false;
        tabla_paginas[i].nro_pagina = -1;
        tabla_paginas[i].frame = memoria_interna + (i * tam_bloque);
    }

    printf("[MEM] Inicializada con %d páginas de %d bytes\n", cantidad_paginas, tam_bloque);
}

// --------------------------------------------------ACCEDER A PAG

void acceder_a_pagina(int nro_pagina) {
    t_config* config = config_create("worker.config");
    pthread_mutex_lock(&mutex_memoria);
    RETARDO_MEMORIA= config_get_int_value(config,"RETARDO_MEMORIA");
    // Buscar la página en memoria
    for (int i = 0; i < cantidad_paginas; i++) {
        if (tabla_paginas[i].en_uso && tabla_paginas[i].nro_pagina == nro_pagina) {
            tabla_paginas[i].bit_uso = true;
            printf("[MEM] Acceso a página %d (bit de uso actualizado)\n", nro_pagina);
            pthread_mutex_unlock(&mutex_memoria);
            usleep(RETARDO_MEMORIA * 1000);
            return;
        }
    }

// Si no está, hay que cargarla (paginación a demanda)
    printf("[MEM] Fallo de página: cargando página %d\n", nro_pagina);

    int frame_victima = reemplazar_pagina_clock();

     // Simulamos escritura al storage si estaba modificada
    if (tabla_paginas[frame_victima].modificado) {
        printf("[MEM] Página %d modificada: escribiendo al Storage antes de reemplazar\n",
               tabla_paginas[frame_victima].nro_pagina);
    }

    //CArgamos la nueva pagina
    tabla_paginas[frame_victima].en_uso = true;
    tabla_paginas[frame_victima].bit_uso = true;
    tabla_paginas[frame_victima].modificado = false;
    tabla_paginas[frame_victima].nro_pagina = nro_pagina;

    // Simulamos carga desde Storage (leer bloque)
    printf("[MEM] Página %d cargada en frame %d\n", nro_pagina, frame_victima);

    pthread_mutex_unlock(&mutex_memoria);
    usleep(RETARDO_MEMORIA * 1000);
}
// ------------------ Algoritmo CLOCK ------------------

int reemplazar_pagina_clock() {
    while (1) {
        if (!tabla_paginas[puntero_clock].en_uso) {
            int seleccionado = puntero_clock;
            puntero_clock = (puntero_clock + 1) % cantidad_paginas;
            return seleccionado;
        }

        if (!tabla_paginas[puntero_clock].bit_uso) {
            int seleccionado = puntero_clock;
            puntero_clock = (puntero_clock + 1) % cantidad_paginas;
            printf("[CLOCK] Reemplazo: frame %d seleccionado\n", seleccionado);
            return seleccionado;
        }

        tabla_paginas[puntero_clock].bit_uso = false;
        puntero_clock = (puntero_clock + 1) % cantidad_paginas;
    }
}

// ------------------ Liberar memoria ------------------

/*void liberar_memoria_interna() {
    free(memoria_interna);
    free(tabla_paginas);
    free(ALGORITMO_REEMPLAZO);
    printf("[MEM] Memoria interna liberada.\n");
}*/

//Para hacer el write o read real debemos invocar a storage para que haga esa operacion