
#include "queryInterpreter.h"

void envioAQueryInterpreter(t_pedido_master_worker* pedido){
  //deberia hacer la logica para que pueda saber que query quiere master que queryinterpreter ejecute.
  //y segun el pc ejecutar desde esa posicion.
  
}

void ejecutarOperacion(Operation op) {// aca deberian llegar tambien como parameto, los parametros de 
    // las funciones.
    switch (op) {
        case CREATE:
            // conexion_storage debe ser global para todo el modulo.
            char* tag = "estaMockiado";
            int confirmacion = enviar_instruccion_a_storage(conexion_storage, tag, 0, CREATE);
            if(confirmacion != 1){
                // LOGEAR UN ERROR
            }
            printf("Ejecutando CREATE\n");
            break;
        case TRUNCATE:
            printf("Ejecutando TRUNCATE\n");
            break;
        case WRITE:
            printf("Ejecutando WRITE\n");
            break;
        case READ:
            printf("Ejecutando READ\n");
            break;
        case TAG:
            printf("Ejecutando TAG\n");
            break;
        case COMMIT:
            printf("Ejecutando COMMIT\n");
            break;
        case FLUSH:
            printf("Ejecutando FLUSH\n");
            break;
        case DELETE:
            printf("Ejecutando DELETE\n");
            break;
        case END:
            printf("Ejecutando END\n");
            break;
        default:
            printf("Operacion desconocida\n");
            break;
    }
}