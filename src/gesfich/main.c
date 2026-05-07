#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pipe_utils.h"
#include "json_utils.h"

#define PIPE_NOMBRE "ctrllt_gesfich"

/* ── Estado del servicio ─────────────────────────────────────────────────── */
typedef enum { EST_CORRIENDO, EST_SUSPENDIDO, EST_TERMINADO } Estado;
static Estado estado_actual = EST_CORRIENDO;

static char aralmac_ruta[512];      /* directorio base de almacenamiento */
static int  contador_id = 1;         /* siguiente número de fichero */
static CRITICAL_SECTION cs_contador; /* protege contador_id */

/* ── Generador de IDs ────────────────────────────────────────────────────── */
static void generar_id(char *out, size_t size) {
    EnterCriticalSection(&cs_contador);
    snprintf(out, size, "f-%04d", contador_id++);
    LeaveCriticalSection(&cs_contador);
}

/* ── Operaciones ─────────────────────────────────────────────────────────── */

static char *op_crear(void) {
    char id[16];
    generar_id(id, sizeof(id));

    /* Crear archivo vacío en aralmac */
    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%s\\%s.dat", aralmac_ruta, id);

    FILE *f = fopen(ruta, "wb");
    if (!f) {
        return json_construir_respuesta("ERROR", NULL, "No se pudo crear el fichero");
    }
    fclose(f);

    cJSON *datos = cJSON_CreateObject();
    cJSON_AddStringToObject(datos, "id", id);
    char *resp = json_construir_respuesta("OK", datos, NULL);
    cJSON_Delete(datos);
    return resp;
}

static char *op_leer(const char *id) {
    if (!id) {
        /* TODO: listar todos los ficheros */
        return json_construir_respuesta("OK", cJSON_CreateArray(), NULL);
    }

    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%s\\%s.dat", aralmac_ruta, id);

    FILE *f = fopen(ruta, "rb");
    if (!f) {
        return json_construir_respuesta("ERROR", NULL, "Fichero no encontrado");
    }

    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    rewind(f);

    char *contenido = calloc(tam + 1, 1);
    fread(contenido, 1, tam, f);
    fclose(f);

    cJSON *datos = cJSON_CreateObject();
    cJSON_AddStringToObject(datos, "id",       id);
    cJSON_AddStringToObject(datos, "contenido", contenido);
    free(contenido);

    char *resp = json_construir_respuesta("OK", datos, NULL);
    cJSON_Delete(datos);
    return resp;
}

static char *op_actualizar(const char *id, const char *ruta_origen) {
    if (!id || !ruta_origen) {
        return json_construir_respuesta("ERROR", NULL, "Faltan parámetros");
    }

    char ruta_dest[512];
    snprintf(ruta_dest, sizeof(ruta_dest), "%s\\%s.dat", aralmac_ruta, id);

    /* Verificar que el fichero destino existe en aralmac */
    if (GetFileAttributesA(ruta_dest) == INVALID_FILE_ATTRIBUTES) {
        return json_construir_respuesta("ERROR", NULL, "ID no encontrado en aralmac");
    }

    if (!CopyFileA(ruta_origen, ruta_dest, FALSE)) {
        return json_construir_respuesta("ERROR", NULL, "No se pudo copiar el fichero");
    }

    return json_construir_respuesta("OK", NULL, NULL);
}

static char *op_borrar(const char *id) {
    if (!id) {
        return json_construir_respuesta("ERROR", NULL, "Falta el id");
    }

    char ruta[512];
    snprintf(ruta, sizeof(ruta), "%s\\%s.dat", aralmac_ruta, id);

    if (!DeleteFileA(ruta)) {
        return json_construir_respuesta("ERROR", NULL, "Fichero no encontrado");
    }

    return json_construir_respuesta("OK", NULL, NULL);
}

/* ── Despachador de operaciones ──────────────────────────────────────────── */
static char *despachar(const char *buf) {
    cJSON *peticion = json_parsear(buf);
    if (!peticion) {
        return json_construir_respuesta("ERROR", NULL, "JSON inválido");
    }

    const char *op = json_get_str(peticion, "operacion");
    if (!op) {
        cJSON_Delete(peticion);
        return json_construir_respuesta("ERROR", NULL, "Falta 'operacion'");
    }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(peticion, "parametros");

    char *resp = NULL;

    if (strcmp(op, "CREAR") == 0) {
        resp = op_crear();

    } else if (strcmp(op, "LEER") == 0) {
        const char *id = params ? json_get_str(params, "id") : NULL;
        resp = op_leer(id);

    } else if (strcmp(op, "ACTUALIZAR") == 0) {
        const char *id   = params ? json_get_str(params, "id")   : NULL;
        const char *ruta = params ? json_get_str(params, "ruta")  : NULL;
        resp = op_actualizar(id, ruta);

    } else if (strcmp(op, "BORRAR") == 0) {
        const char *id = params ? json_get_str(params, "id") : NULL;
        resp = op_borrar(id);

    } else if (strcmp(op, "SUSPENDER") == 0) {
        estado_actual = EST_SUSPENDIDO;
        resp = json_construir_respuesta("OK", NULL, "Suspendido");

    } else if (strcmp(op, "REASUMIR") == 0) {
        estado_actual = EST_CORRIENDO;
        resp = json_construir_respuesta("OK", NULL, "Corriendo");

    } else if (strcmp(op, "TERMINAR") == 0) {
        estado_actual = EST_TERMINADO;
        resp = json_construir_respuesta("OK", NULL, "Terminado");

    } else {
        resp = json_construir_respuesta("ERROR", NULL, "Operación desconocida");
    }

    cJSON_Delete(peticion);
    return resp;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    /* Leer argumento -x <ruta-aralmac> */
    const char *ruta_arg = NULL;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-x") == 0) {
            ruta_arg = argv[i + 1];
        }
    }
    if (!ruta_arg) {
        ruta_arg = "aralmac\\ficheros";  /* valor por defecto */
    }
    strncpy(aralmac_ruta, ruta_arg, sizeof(aralmac_ruta) - 1);

    /* Normalizar barras a Windows */
    for (char *p = aralmac_ruta; *p; p++) {
        if (*p == '/') *p = '\\';
    }
    CreateDirectoryA(aralmac_ruta, NULL);  /* crear si no existe */

    InitializeCriticalSection(&cs_contador);

    printf("[gesfich] Iniciando. aralmac='%s'\n", aralmac_ruta);

    HANDLE hPipe = pipe_crear_servidor(PIPE_NOMBRE);
    if (hPipe == INVALID_HANDLE_VALUE) return 1;

    if (pipe_aceptar_cliente(hPipe) < 0) return 1;
    printf("[gesfich] ctrllt conectado.\n");

    char buf[PIPE_BUF_SIZE];

    while (estado_actual != EST_TERMINADO) {
        if (estado_actual == EST_SUSPENDIDO) {
            Sleep(200);
            continue;
        }

        int n = pipe_recibir(hPipe, buf, sizeof(buf));
        if (n <= 0) break;

        printf("[gesfich] %s\n", buf);

        char *resp = despachar(buf);
        pipe_enviar(hPipe, resp);
        json_liberar_str(resp);
    }

    pipe_cerrar(hPipe);
    DeleteCriticalSection(&cs_contador);
    printf("[gesfich] Terminado.\n");
    return 0;
}
