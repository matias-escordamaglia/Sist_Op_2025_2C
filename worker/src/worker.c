#include <worker.h>


char* archivo_config;
uint32_t id_worker;

pthread_t hilo_master;
pthread_t hilo_storage;

pthread_t hilo_lanzamiento_ejecucion;

int main(int argc, char** argv)
{	

	if (argc < 3) {
        printf("Uso correcto: %s <archivo_config[path]> <ID_Worker[int]>\n", argv[0]);
        return EXIT_FAILURE;
    }

	archivo_config = argv[1];
    id_worker = atoi(argv[2]);

	config = iniciar_config(logger, archivo_config);

	log_level = obtener_log_level_config(config);

	logger = log_create("worker.log", "WORKER", true, log_level);


    iniciar_semaforos();
    settear_valores_nulos_query_actual();

    pthread_create(&hilo_lanzamiento_ejecucion, NULL, main_lanzamiento_ejecucion, NULL);
    pthread_detach(hilo_lanzamiento_ejecucion);


    ip_storage = config_get_string_value(config, "IP_STORAGE");
	puerto_storage = config_get_string_value(config, "PUERTO_STORAGE");


	ip_master = config_get_string_value(config, "IP_MASTER");
	puerto_master = config_get_string_value(config, "PUERTO_MASTER");

    conexion_storage = crear_conexion(ip_storage, puerto_storage, logger);

    if (conexion_storage == -1) {
        log_error(logger, "No se pudo establecer conexión con STORAGE. Abortando.");
        terminar_programa(conexion_storage, -99, logger, config);
        exit(EXIT_FAILURE);
    }
    char* base = config_get_string_value(config, "PATH_SCRIPTS");
    cargar_scripts(base, logger);
    

    handshake_con_identificador_worker(conexion_storage, 1, id_worker, logger, "STORAGE");
    recv(conexion_storage, &block_size, sizeof(int), MSG_WAITALL);
    // pasar_bloque_a_memoria(&block_size);
    //inicializacion memoria
    iniciar_memoria_interna(config);
    
    log_info(logger, "Memoria interna inicializada correctamente.");

	conexion_master = crear_conexion(ip_master, puerto_master, logger);

    if (conexion_master == -1) {
        log_error(logger, "No se pudo establecer conexión con el MASTER. Abortando.");
        terminar_programa(conexion_master, conexion_storage, logger, config);
        exit(EXIT_FAILURE);
    }
    
    handshake_con_identificador_worker(conexion_master, 1, id_worker, logger, "MASTER");

    int* server_fd_copia_storage = malloc(sizeof(int));
    *server_fd_copia_storage = conexion_storage;
	pthread_create(&hilo_storage, NULL, manejar_storage, server_fd_copia_storage);
	
    int* server_fd_copia_master = malloc(sizeof(int));
    *server_fd_copia_master = conexion_master;
    pthread_create(&hilo_master, NULL, manejar_master, server_fd_copia_master);

	/*
	Lo siguiente debe ajustarse para cada modulo
	*/
	pthread_join(hilo_storage, NULL);
	pthread_join(hilo_master, NULL);

    destruir_semaforos();
    
    //liberar memoria al final
    destroy_memoria_interna();

    return 0;
}

void cargar_scripts(const char* path_base, t_log* logger){
    DIR* dir = opendir(path_base);
    if (!dir) {log_error(logger, "No se pudo abrir %s", path_base); return; }
    if (!diccionario_programas) diccionario_programas = dictionary_create();

    struct dirent* e;
    char ruta[4096];
    while ((e = readdir(dir)) != NULL){
        if (e->d_type != DT_REG) continue;

        if (snprintf(ruta, sizeof(ruta), "%s/%s", path_base, e->d_name) >= (int)sizeof(ruta)){
            log_error(logger, "Ruta demasiado larga: %s/%s", path_base, e->d_name);
            continue;
        }

        t_programa* prog = leer_y_partir(ruta);
        if (!prog){ log_error(logger, "No se pudo leer %s", ruta); continue; }

        dictionary_put(diccionario_programas, strdup(e->d_name), prog);
        log_info(logger, "Script registrado: %s (instrucciones=%zu)", e->d_name, prog->cant);
    }
    closedir(dir);
}

void* manejar_storage(void* arg) {
    int conexion = *((int*)arg);
    free(arg);
    
    log_info(logger, " Worker - STORAGE conectado  - FD del socket: %d", conexion);

    while (1) {	
        int cod_op = recibir_operacion(conexion, logger);
        if (cod_op == -1) {
            log_info(logger, "STORAGE desconectado");
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                int size;
                void* buffer = recibir_buffer(&size, conexion);
                if (buffer == NULL) {
                    log_error(logger, "Error al recibir el buffer de STORAGE");
                    return NULL;
                }
                

                break;

            default:
                log_warning(logger, "Código de operación desconocido de QUERY: %d", cod_op);
                break;
        }
    }

    close(conexion);
    return NULL;
}

void* manejar_master(void* arg) {
    int conexion = *((int*)arg);
    free(arg);
    
    log_info(logger, " Worker - MASTER conectado  - FD del socket: %d", conexion);

    while (1) {
        int cod_op = recibir_operacion(conexion, logger);
        if (cod_op == -1) {
            log_info(logger, "MASTER desconectado");
            break;
        }
        
        switch (cod_op) {
            case MENSAJE:
                
                //Realizar cosas en caso que llegue un mensaje (o tratarlo como error)

                break;
                
            case PAQUETE:

                int size;
                void* buffer = recibir_buffer(&size, conexion);
                if (buffer == NULL) {
                    log_error(logger, "Error al recibir el buffer de MASTER");
                    return NULL;
                }
                
                t_pedido_master_worker* pedido = desempaquetar_pedido_master_worker(buffer);
                
                if (!pedido) {
                    log_error(logger, "Error al desempaquetar pedido de MASTER");
                    free(buffer);
                    break;
                }

                t_aviso_worker_master* aviso_confirmacion = malloc(sizeof(t_aviso_worker_master));

                switch (pedido->motivo)
                {
                case PEDIDO_QUERY:

                    

                    if(query_actual.qid_actual == QID_NULO) {

                        aviso_confirmacion->tipo_aviso = RESPUESTA_SIG_QUERY;
                        aviso_confirmacion->argumento = convertir_int_a_string(OK);


                        if(!armar_y_enviar_confirmacion_a_master(aviso_confirmacion, conexion)){
                            log_error(logger, "No se pudo empaquetar el aviso de confirmación a Master");
                        }

                        // Parte Testing
                        char* mensaje  = "PRUEBA:VERSION1.0 Lectura_de_prueba"; 
                        t_tipo_aviso_worker_master tipo_aviso = NUEVA_LECTURA;
                        t_paquete* paquete_resp = crear_paquete();
            
                        insertar_variable_a_paquete(paquete_resp, &(tipo_aviso), sizeof(t_tipo_aviso_worker_master));
                        insertar_string_a_paquete(paquete_resp, mensaje);
                        enviar_paquete(paquete_resp,conexion);
                        // Fin Testing
                        
                        t_motivo_pedido_master_worker motivo = pedido->motivo;
                        query_actual.query_path = pedido->query_path;
                        query_actual.pc_actual = pedido->program_counter;
                        query_actual.qid_actual = pedido->query_id;

                        log_info(logger, "Nuevo pedido de Query. Query ID: %d - Path: %s - Program Count: %d - Motivo: %d ", 
                            query_actual.qid_actual, query_actual.query_path, query_actual.pc_actual, motivo);

                        sem_post(sem_ejecucion_pendiente);

                        

                    } else {
                        

                        aviso_confirmacion->tipo_aviso = RESPUESTA_SIG_QUERY;
                        aviso_confirmacion->argumento = convertir_int_a_string(OK);

                        if(!armar_y_enviar_confirmacion_a_master(aviso_confirmacion, conexion)){
                            log_error(logger, "No se pudo empaquetar el aviso de rechazo de pedido a Master");
                        }
                    }


                    break;
                    
                    
                case INTERRUPCION:
                    

                    if(query_actual.qid_actual != pedido->query_id) {
                        aviso_confirmacion->tipo_aviso = DESALOJO_QUERY_DIFERENTE_RESPUESTA;
                        int qid_temp = query_actual.qid_actual;
                        aviso_confirmacion->argumento = convertir_int_a_string(qid_temp);

                        if(!armar_y_enviar_confirmacion_a_master(aviso_confirmacion, conexion)){
                            log_error(logger, "No se pudo empaquetar el aviso de query diferente a Master");
                        }

                    } else {
                        hay_pedido_desalojo = true;

                        sem_wait(sem_desalojo_pendiente);

                        
                        if (hay_pedido_desalojo) {
        
                            aviso_confirmacion->tipo_aviso = DEVOLUCION_X_INTERRUPCION;
                            int pc_temp = query_actual.pc_actual; 
                            aviso_confirmacion->argumento = convertir_int_a_string(pc_temp);

                            if(!armar_y_enviar_confirmacion_a_master(aviso_confirmacion, conexion)){
                                log_error(logger, "Error enviando aviso de interrupcion a Master");
                            }
                            
                            
                            hay_pedido_desalojo = false;
                            
                            settear_valores_nulos_query_actual();
                        
                        } else {
                            log_warning(logger, "Omitiendo envío de interrupción: la query finalizó por END concurrentemente.");
                        }

                    }

                    break;
                
                default:
                    log_error(logger, "Error; código de motivo de pedido master desconocido");
                    break;
                }
                
               
                free(pedido->query_path);
                free(pedido);
                free(buffer); 

                break;
                
            default:
                log_warning(logger, "Código de operación desconocido de MASTER: %d", cod_op);
                break;
        }
    }

    close(conexion);
    return NULL;
}

void handshake_con_identificador_worker(int socket, int valor ,uint32_t id_worker, t_log* logger, char* nombre_modulo) {
    if (handshake(socket, valor, logger, nombre_modulo) == (uint32_t)-1) {
        log_error(logger, "Handshake fallido con %s", nombre_modulo);
        exit(EXIT_FAILURE);
    }

    t_estado_handshake estado_handshake;

    send(socket, &id_worker, sizeof(uint32_t), 0);

    if (recv(socket, &estado_handshake, sizeof(t_estado_handshake), MSG_WAITALL) <= 0) {
        log_error(logger, "No se recibió respuesta de %s tras enviar el ID de WORKER", nombre_modulo);
        exit(EXIT_FAILURE);
    }
        

    if (estado_handshake == HANDSHAKE_OK) {
        log_info(logger, "WORKER %u registrado correctamente en %s", id_worker, nombre_modulo);
    } else {
        log_error(logger, "WORKER %u ya estaba registrado en %s. Abortando...", id_worker, nombre_modulo);
        exit(EXIT_FAILURE);
    }
}

void terminar_programa(int conexion1, int conexion2, t_log* logger, t_config* config) {
    
	log_info(logger, "Finalizando programa...");
    
	log_destroy(logger);
    config_destroy(config);
    
	close(conexion1);
	if(conexion2 != -99 ) {
		close(conexion2);
	}
}

void rstrip(char* s){
    size_t n = strlen(s);
    while (n && (s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t')) s[--n]='\0';
}

bool vacia_o_coment(const char* s){
    while (*s==' '||*s=='\t') s++;
    return (*s=='\0' || *s=='#' || (*s=='/' && *(s+1)=='/'));
}

t_programa* leer_y_partir(const char* path){
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    t_programa* p = calloc(1, sizeof(*p));
    if (!p){ fclose(f); return NULL; }

    char* line = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&line, &cap, f)) != -1){
        (void)n;
        rstrip(line);
        if (vacia_o_coment(line)) continue;

        char* dup = strdup(line);
        if (!dup){ fclose(f); free(line); return p; } // dejamos lo cargado hasta ahora

        char** nuevo = realloc(p->instrucciones, (p->cant+1)*sizeof(char*));
        if (!nuevo){ free(dup); fclose(f); free(line); return p; }
        p->instrucciones = nuevo;
        p->instrucciones[p->cant++] = dup;
    }
    free(line);
    fclose(f);
    return p;
}

bool armar_y_enviar_confirmacion_a_master(t_aviso_worker_master* aviso_confirmacion, int conexion) {
    t_paquete* paquete_confirmacion = empaquetar_aviso_worker_master(aviso_confirmacion);

    if (!paquete_confirmacion) {
        free(aviso_confirmacion->argumento);
        free(aviso_confirmacion);
        return false;
    }

    enviar_paquete(paquete_confirmacion, conexion);

    free(aviso_confirmacion->argumento);
    free(aviso_confirmacion);

    return true;
}

