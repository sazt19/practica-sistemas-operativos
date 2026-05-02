#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pipe_utils.h"
#include "json_utils.h"

#define PIPE_CTRLLT "ctrllt_cliente"

static void uso(void) {
    printf("Uso: cliente <servicio> <operacion> [parametros_json]\n");
    printf("\nEjemplos:\n");
    printf("  cliente gesfich CREAR\n");
    printf("  cliente gesfich LEER   '{\"id\":\"f-0001\"}'\n");
    printf("  cliente gesfich BORRAR '{\"id\":\"f-0001\"}'\n");
    printf("  cliente gesprog GUARDAR '{\"ejecutable\":\"C:\\\\prog.exe\",\"argumentos\":[],\"ambiente\":[]}'\n");
    printf("  cliente ejecutor EJECUTAR '{\"id_programa\":\"p-0001\",\"fichero_entrada\":\"f-0001\",\"fichero_salida\":\"f-0002\"}'\n");
    printf("  cliente ejecutor ESTADO  '{\"id_lote\":\"l-0001\"}'\n");
    printf("  cliente ctrllt TERMINAR\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) { uso(); return 1; }

    const char *servicio  = argv[1];
    const char *operacion = argv[2];
    const char *params_str = argc >= 4 ? argv[3] : NULL;

    /* Conectar a ctrllt */
    HANDLE h = pipe_conectar_cliente(PIPE_CTRLLT);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: no se pudo conectar a ctrllt\n");
        return 1;
    }

    /* Construir petición */
    cJSON *params = params_str ? json_parsear(params_str) : NULL;
    char *peticion = json_construir_peticion(servicio, operacion, params);

    printf("Enviando: %s\n", peticion);
    pipe_enviar(h, peticion);
    json_liberar_str(peticion);
    if (params) cJSON_Delete(params);

    /* Leer respuesta */
    char buf[PIPE_BUF_SIZE];
    int n = pipe_recibir(h, buf, sizeof(buf));
    if (n > 0) {
        printf("Respuesta: %s\n", buf);
    } else {
        printf("Sin respuesta (conexión cerrada)\n");
    }

    pipe_cerrar(h);
    return 0;
}
