#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pipe_utils.h"
#include "json_utils.h"

#define PIPE_NOMBRE "ctrllt_gesprog"

typedef enum { EST_CORRIENDO, EST_SUSPENDIDO, EST_TERMINADO } Estado;
static Estado estado_actual = EST_CORRIENDO;

static char aralmac_ruta[512];
static int  contador_id = 1;
static CRITICAL_SECTION cs_contador;

static void generar_id(char *out, size_t size) {
    EnterCriticalSection(&cs_contador);
    snprintf(out, size, "p-%04d", contador_id++);
    LeaveCriticalSection(&cs_contador);
}

/* ── Operaciones ─────────────────────────────────────────────────────────── */

static char *op_guardar(cJSON *params) {
    if (!params) {
        return json_construir_respuesta("ERROR", NULL, "Faltan parámetros");
    }

    const char *ejecutable = json_get_str(params, "ejecutable");
    if (!ejecutable) {
        return json_construir_respuesta("ERROR", NULL, "Falta 'ejecutable'");
    }

    /* Verificar que el ejecutable existe */
    if (GetFileAttributesA(ejecutable) == INVALID_FILE_ATTRIBUTES) {
        return json_construir_respuesta("ERROR", NULL, "Ejecutable no encontrado");
    }

    char id[16];
    generar_id(id, sizeof(id));

    /* Directorio del programa en aralmac */
    char dir_prog[512];
    snprintf(dir_prog, sizeof(dir_prog), "%s\\%s", aralmac_ruta, id);
    CreateDirectoryA(dir_prog, NULL);

    /* Copiar el ejecutable */
    char dest_exe[512];
    const char *p1 = strrchr(ejecutable, '\\');
    const char *p2 = strrchr(ejecutable, '/');
    const char *nombre_exe;
    if (p1 && p2) nombre_exe = (p1 > p2 ? p1 : p2) + 1;
    else if (p1)  nombre_exe = p1 + 1;
    else if (p2)  nombre_exe = p2 + 1;
    else          nombre_exe = ejecutable;
    snprintf(dest_exe, sizeof(dest_exe), "%s\\%s", dir_prog, nombre_exe);
    CopyFileA(ejecutable, dest_exe, FALSE);

    /* Guardar metadatos como JSON en un archivo .meta */
    char meta_ruta[512];
    snprintf(meta_ruta, sizeof(meta_ruta), "%s\\meta.json", dir_prog);

    cJSON *meta = cJSON_CreateObject();
    cJSON_AddStringToObject(meta, "id",         id);
    cJSON_AddStringToObject(meta, "ejecutable", dest_exe);

    cJSON *args = cJSON_GetObjectItemCaseSensitive(params, "argumentos");
    cJSON *env  = cJSON_GetObjectItemCaseSensitive(params, "ambiente");
    cJSON_AddItemReferenceToObject(meta, "argumentos", args ? args : cJSON_CreateArray());
    cJSON_AddItemReferenceToObject(meta, "ambiente",   env  ? env  : cJSON_CreateArray());

    char *meta_str = cJSON_Print(meta);
    FILE *f = fopen(meta_ruta, "w");
    if (f) { fputs(meta_str, f); fclose(f); }
    free(meta_str);
    cJSON_Delete(meta);

    cJSON *datos = cJSON_CreateObject();
    cJSON_AddStringToObject(datos, "id", id);
    char *resp = json_construir_respuesta("OK", datos, NULL);
    cJSON_Delete(datos);
    return resp;
}

static char *op_leer(const char *id) {
    if (!id) {
        /* TODO: listar todos los programas */
        return json_construir_respuesta("OK", cJSON_CreateArray(), NULL);
    }

    char meta_ruta[512];
    snprintf(meta_ruta, sizeof(meta_ruta), "%s\\%s\\meta.json", aralmac_ruta, id);

    FILE *f = fopen(meta_ruta, "r");
    if (!f) {
        return json_construir_respuesta("ERROR", NULL, "Programa no encontrado");
    }

    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    rewind(f);
    char *contenido = calloc(tam + 1, 1);
    fread(contenido, 1, tam, f);
    fclose(f);

    cJSON *datos = json_parsear(contenido);
    free(contenido);

    char *resp = json_construir_respuesta("OK", datos, NULL);
    cJSON_Delete(datos);
    return resp;
}

static char *op_borrar(const char *id) {
    if (!id) return json_construir_respuesta("ERROR", NULL, "Falta el id");

    char dir_prog[512];
    snprintf(dir_prog, sizeof(dir_prog), "%s\\%s", aralmac_ruta, id);

    if (GetFileAttributesA(dir_prog) == INVALID_FILE_ATTRIBUTES) {
        return json_construir_respuesta("ERROR", NULL, "Programa no encontrado");
    }

    /* TODO: borrar directorio recursivamente */
    return json_construir_respuesta("OK", NULL, "Borrado (implementar recursivo)");
}

/* ── Despachador ─────────────────────────────────────────────────────────── */
static char *despachar(const char *buf) {
    cJSON *peticion = json_parsear(buf);
    if (!peticion) return json_construir_respuesta("ERROR", NULL, "JSON inválido");

    const char *op = json_get_str(peticion, "operacion");
    if (!op) { cJSON_Delete(peticion); return json_construir_respuesta("ERROR", NULL, "Falta 'operacion'"); }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(peticion, "parametros");
    char *resp = NULL;

    if      (strcmp(op, "GUARDAR")   == 0) resp = op_guardar(params);
    else if (strcmp(op, "LEER")      == 0) resp = op_leer(params ? json_get_str(params, "id") : NULL);
    else if (strcmp(op, "BORRAR")    == 0) resp = op_borrar(params ? json_get_str(params, "id") : NULL);
    else if (strcmp(op, "SUSPENDER") == 0) { estado_actual = EST_SUSPENDIDO; resp = json_construir_respuesta("OK", NULL, "Suspendido"); }
    else if (strcmp(op, "REASUMIR")  == 0) { estado_actual = EST_CORRIENDO;  resp = json_construir_respuesta("OK", NULL, "Corriendo"); }
    else if (strcmp(op, "TERMINAR")  == 0) { estado_actual = EST_TERMINADO;  resp = json_construir_respuesta("OK", NULL, "Terminado"); }
    else resp = json_construir_respuesta("ERROR", NULL, "Operación desconocida");

    cJSON_Delete(peticion);
    return resp;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    const char *ruta_arg = NULL;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-x") == 0) ruta_arg = argv[i + 1];
    }
    if (!ruta_arg) ruta_arg = "aralmac\\programas";
    strncpy(aralmac_ruta, ruta_arg, sizeof(aralmac_ruta) - 1);

    /* Normalizar barras a Windows */
    for (char *p = aralmac_ruta; *p; p++) {
        if (*p == '/') *p = '\\';
    }
    CreateDirectoryA(aralmac_ruta, NULL);

    InitializeCriticalSection(&cs_contador);
    printf("[gesprog] Iniciando. aralmac='%s'\n", aralmac_ruta);

    HANDLE hPipe = pipe_crear_servidor(PIPE_NOMBRE);
    if (hPipe == INVALID_HANDLE_VALUE) return 1;
    if (pipe_aceptar_cliente(hPipe) < 0) return 1;
    printf("[gesprog] ctrllt conectado.\n");

    char buf[PIPE_BUF_SIZE];
    while (estado_actual != EST_TERMINADO) {
        if (estado_actual == EST_SUSPENDIDO) { Sleep(200); continue; }
        int n = pipe_recibir(hPipe, buf, sizeof(buf));
        if (n <= 0) break;
        printf("[gesprog] %s\n", buf);
        char *resp = despachar(buf);
        pipe_enviar(hPipe, resp);
        json_liberar_str(resp);
    }

    pipe_cerrar(hPipe);
    DeleteCriticalSection(&cs_contador);
    printf("[gesprog] Terminado.\n");
    return 0;
}
