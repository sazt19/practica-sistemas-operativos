#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pipe_utils.h"
#include "json_utils.h"

#define PIPE_NOMBRE    "ctrllt_ejecutor"
#define MAX_LOTES      64

typedef enum { EST_EJECUTAR, EST_SUSPENDIDOS, EST_PARAR, EST_TERMINADO } Estado;
static Estado estado_actual = EST_EJECUTAR;

static char aralmac_ruta[512];
static int  contador_lote = 1;
static CRITICAL_SECTION cs_lotes;

/* ── Registro de procesos de lotes ───────────────────────────────────────── */
typedef enum { LOTE_CORRIENDO, LOTE_TERMINADO, LOTE_ERROR } EstadoLote;

typedef struct {
    char          id[16];
    HANDLE        hProceso;
    EstadoLote    estado;
} Lote;

static Lote lotes[MAX_LOTES];
static int  num_lotes = 0;

static void generar_id_lote(char *out, size_t size) {
    EnterCriticalSection(&cs_lotes);
    snprintf(out, size, "l-%04d", contador_lote++);
    LeaveCriticalSection(&cs_lotes);
}

/* ── Operaciones ─────────────────────────────────────────────────────────── */

static char *op_ejecutar(cJSON *params) {
    if (!params) return json_construir_respuesta("ERROR", NULL, "Faltan parámetros");

    const char *id_prog   = json_get_str(params, "id_programa");
    const char *id_fich_e = json_get_str(params, "fichero_entrada");
    const char *id_fich_s = json_get_str(params, "fichero_salida");

    if (!id_prog || !id_fich_e || !id_fich_s) {
        return json_construir_respuesta("ERROR", NULL,
            "Faltan id_programa, fichero_entrada o fichero_salida");
    }

    /* Leer metadatos del programa desde aralmac */
    char meta_ruta[512];
    snprintf(meta_ruta, sizeof(meta_ruta),
             "%s\\..\\programas\\%s\\meta.json", aralmac_ruta, id_prog);

    FILE *fm = fopen(meta_ruta, "r");
    if (!fm) return json_construir_respuesta("ERROR", NULL, "Programa no encontrado");

    fseek(fm, 0, SEEK_END); long tam = ftell(fm); rewind(fm);
    char *meta_str = calloc(tam + 1, 1);
    fread(meta_str, 1, tam, fm);
    fclose(fm);

    cJSON *meta = json_parsear(meta_str);
    free(meta_str);
    if (!meta) return json_construir_respuesta("ERROR", NULL, "Metadatos inválidos");

    const char *ejecutable = json_get_str(meta, "ejecutable");
    if (!ejecutable) { cJSON_Delete(meta); return json_construir_respuesta("ERROR", NULL, "Sin ejecutable"); }

    /* Rutas de stdin/stdout en aralmac */
    char ruta_stdin[512], ruta_stdout[512];
    snprintf(ruta_stdin,  sizeof(ruta_stdin),  "%s\\%s.dat", aralmac_ruta, id_fich_e);
    snprintf(ruta_stdout, sizeof(ruta_stdout), "%s\\%s.dat", aralmac_ruta, id_fich_s);

    /* Abrir handles de fichero para redirección */
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    HANDLE hStdin = CreateFileA(ruta_stdin, GENERIC_READ, FILE_SHARE_READ,
                                &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE hStdout = CreateFileA(ruta_stdout, GENERIC_WRITE, 0,
                                 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
        cJSON_Delete(meta);
        return json_construir_respuesta("ERROR", NULL, "No se pudo abrir ficheros de E/S");
    }

    /* Configurar STARTUPINFO con redirección */
    STARTUPINFOA si = { 0 };
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = hStdin;
    si.hStdOutput  = hStdout;
    si.hStdError   = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = { 0 };

    /* Construir línea de comandos: ejecutable + argumentos */
    char cmdline[1024];
    strncpy(cmdline, ejecutable, sizeof(cmdline) - 1);

    cJSON *args = cJSON_GetObjectItemCaseSensitive(meta, "argumentos");
    if (args) {
        cJSON *arg;
        cJSON_ArrayForEach(arg, args) {
            strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
            strncat(cmdline, arg->valuestring, sizeof(cmdline) - strlen(cmdline) - 1);
        }
    }

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL,
                             TRUE,   /* heredar handles */
                             0, NULL, NULL, &si, &pi);

    CloseHandle(hStdin);
    CloseHandle(hStdout);
    cJSON_Delete(meta);

    if (!ok) {
        return json_construir_respuesta("ERROR", NULL, "CreateProcess falló");
    }

    /* Registrar el lote */
    char id_lote[16];
    generar_id_lote(id_lote, sizeof(id_lote));

    EnterCriticalSection(&cs_lotes);
    if (num_lotes < MAX_LOTES) {
        strncpy(lotes[num_lotes].id, id_lote, sizeof(lotes[0].id));
        lotes[num_lotes].hProceso = pi.hProcess;
        lotes[num_lotes].estado   = LOTE_CORRIENDO;
        num_lotes++;
    }
    LeaveCriticalSection(&cs_lotes);

    CloseHandle(pi.hThread);  /* no necesitamos el hilo del proceso hijo */

    cJSON *datos = cJSON_CreateObject();
    cJSON_AddStringToObject(datos, "id_lote", id_lote);
    char *resp = json_construir_respuesta("OK", datos, NULL);
    cJSON_Delete(datos);
    return resp;
}

static char *op_estado(const char *id_lote) {
    EnterCriticalSection(&cs_lotes);

    if (!id_lote) {
        /* Listar todos */
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < num_lotes; i++) {
            /* Actualizar estado real del proceso */
            DWORD exit_code;
            GetExitCodeProcess(lotes[i].hProceso, &exit_code);
            if (exit_code != STILL_ACTIVE) lotes[i].estado = LOTE_TERMINADO;

            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "id", lotes[i].id);
            cJSON_AddStringToObject(item, "estado",
                lotes[i].estado == LOTE_CORRIENDO ? "CORRIENDO" : "TERMINADO");
            cJSON_AddItemToArray(arr, item);
        }
        LeaveCriticalSection(&cs_lotes);
        char *resp = json_construir_respuesta("OK", arr, NULL);
        cJSON_Delete(arr);
        return resp;
    }

    for (int i = 0; i < num_lotes; i++) {
        if (strcmp(lotes[i].id, id_lote) == 0) {
            DWORD exit_code;
            GetExitCodeProcess(lotes[i].hProceso, &exit_code);
            if (exit_code != STILL_ACTIVE) lotes[i].estado = LOTE_TERMINADO;

            const char *est_str = lotes[i].estado == LOTE_CORRIENDO ? "CORRIENDO" : "TERMINADO";
            LeaveCriticalSection(&cs_lotes);

            cJSON *datos = cJSON_CreateObject();
            cJSON_AddStringToObject(datos, "id_lote", id_lote);
            cJSON_AddStringToObject(datos, "estado",  est_str);
            char *resp = json_construir_respuesta("OK", datos, NULL);
            cJSON_Delete(datos);
            return resp;
        }
    }

    LeaveCriticalSection(&cs_lotes);
    return json_construir_respuesta("ERROR", NULL, "ID de lote no encontrado");
}

static char *op_matar(const char *id_lote) {
    if (!id_lote) return json_construir_respuesta("ERROR", NULL, "Falta id_lote");

    EnterCriticalSection(&cs_lotes);
    for (int i = 0; i < num_lotes; i++) {
        if (strcmp(lotes[i].id, id_lote) == 0) {
            TerminateProcess(lotes[i].hProceso, 1);
            lotes[i].estado = LOTE_TERMINADO;
            LeaveCriticalSection(&cs_lotes);
            return json_construir_respuesta("OK", NULL, "Proceso terminado");
        }
    }
    LeaveCriticalSection(&cs_lotes);
    return json_construir_respuesta("ERROR", NULL, "ID de lote no encontrado");
}

/* ── Despachador ─────────────────────────────────────────────────────────── */
static char *despachar(const char *buf) {
    cJSON *peticion = json_parsear(buf);
    if (!peticion) return json_construir_respuesta("ERROR", NULL, "JSON inválido");

    const char *op = json_get_str(peticion, "operacion");
    if (!op) { cJSON_Delete(peticion); return json_construir_respuesta("ERROR", NULL, "Falta 'operacion'"); }

    cJSON *params = cJSON_GetObjectItemCaseSensitive(peticion, "parametros");
    char *resp = NULL;

    if      (strcmp(op, "EJECUTAR")  == 0) resp = op_ejecutar(params);
    else if (strcmp(op, "ESTADO")    == 0) resp = op_estado(params ? json_get_str(params, "id_lote") : NULL);
    else if (strcmp(op, "MATAR")     == 0) resp = op_matar(params ? json_get_str(params, "id_lote") : NULL);
    else if (strcmp(op, "SUSPENDER") == 0) { estado_actual = EST_SUSPENDIDOS; resp = json_construir_respuesta("OK", NULL, "Suspendido"); }
    else if (strcmp(op, "REASUMIR")  == 0) { estado_actual = EST_EJECUTAR;    resp = json_construir_respuesta("OK", NULL, "Corriendo"); }
    else if (strcmp(op, "PARAR")     == 0) { estado_actual = EST_PARAR;       resp = json_construir_respuesta("OK", NULL, "Parando"); }
    else if (strcmp(op, "TERMINAR")  == 0) { estado_actual = EST_TERMINADO;   resp = json_construir_respuesta("OK", NULL, "Terminado"); }
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
    if (!ruta_arg) ruta_arg = "aralmac\\ficheros";
    strncpy(aralmac_ruta, ruta_arg, sizeof(aralmac_ruta) - 1);

    InitializeCriticalSection(&cs_lotes);
    printf("[ejecutor] Iniciando. aralmac='%s'\n", aralmac_ruta);

    HANDLE hPipe = pipe_crear_servidor(PIPE_NOMBRE);
    if (hPipe == INVALID_HANDLE_VALUE) return 1;
    if (pipe_aceptar_cliente(hPipe) < 0) return 1;
    printf("[ejecutor] ctrllt conectado.\n");

    char buf[PIPE_BUF_SIZE];
    while (estado_actual != EST_TERMINADO && estado_actual != EST_PARAR) {
        if (estado_actual == EST_SUSPENDIDOS) { Sleep(200); continue; }
        int n = pipe_recibir(hPipe, buf, sizeof(buf));
        if (n <= 0) break;
        printf("[ejecutor] %s\n", buf);
        char *resp = despachar(buf);
        pipe_enviar(hPipe, resp);
        json_liberar_str(resp);
    }

    /* Limpiar handles de procesos */
    for (int i = 0; i < num_lotes; i++) CloseHandle(lotes[i].hProceso);

    pipe_cerrar(hPipe);
    DeleteCriticalSection(&cs_lotes);
    printf("[ejecutor] Terminado.\n");
    return 0;
}
