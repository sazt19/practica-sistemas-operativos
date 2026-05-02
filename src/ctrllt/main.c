#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "pipe_utils.h"
#include "json_utils.h"

/* ── Configuración de tuberías ──────────────────────────────────────────────
 * ctrllt crea el lado servidor de la pipe con el cliente,
 * y actúa como cliente hacia gesfich, gesprog y ejecutor.
 * ─────────────────────────────────────────────────────────────────────────── */
#define PIPE_CLIENTE  "ctrllt_cliente"
#define PIPE_GESFICH  "ctrllt_gesfich"
#define PIPE_GESPROG  "ctrllt_gesprog"
#define PIPE_EJECUTOR "ctrllt_ejecutor"

/* Handles globales hacia los servicios */
static HANDLE h_gesfich  = INVALID_HANDLE_VALUE;
static HANDLE h_gesprog  = INVALID_HANDLE_VALUE;
static HANDLE h_ejecutor = INVALID_HANDLE_VALUE;

/* ── Estructura para pasar a cada hilo de cliente ────────────────────────── */
typedef struct {
    HANDLE hCliente;
} ClienteArgs;

/* ── Ruteo de petición al servicio correspondiente ───────────────────────── */
static HANDLE rutear(const char *servicio) {
    if (strcmp(servicio, "gesfich")  == 0) return h_gesfich;
    if (strcmp(servicio, "gesprog")  == 0) return h_gesprog;
    if (strcmp(servicio, "ejecutor") == 0) return h_ejecutor;
    return INVALID_HANDLE_VALUE;
}

/* ── Hilo que atiende un cliente ─────────────────────────────────────────── */
static DWORD WINAPI hilo_cliente(LPVOID arg) {
    ClienteArgs *ca = (ClienteArgs *)arg;
    HANDLE hCliente = ca->hCliente;
    free(ca);

    char buf[PIPE_BUF_SIZE];

    while (1) {
        int n = pipe_recibir(hCliente, buf, sizeof(buf));
        if (n <= 0) break;  /* cliente desconectado */

        printf("[ctrllt] Petición: %s\n", buf);

        /* Parsear JSON para determinar el servicio destino */
        cJSON *peticion = json_parsear(buf);
        if (!peticion) {
            char *err = json_construir_respuesta("ERROR", NULL, "JSON inválido");
            pipe_enviar(hCliente, err);
            json_liberar_str(err);
            continue;
        }

        const char *servicio = json_get_str(peticion, "servicio");
        if (!servicio) {
            char *err = json_construir_respuesta("ERROR", NULL, "Falta campo 'servicio'");
            pipe_enviar(hCliente, err);
            json_liberar_str(err);
            cJSON_Delete(peticion);
            continue;
        }

        HANDLE hServicio = rutear(servicio);
        if (hServicio == INVALID_HANDLE_VALUE) {
            char *err = json_construir_respuesta("ERROR", NULL, "Servicio desconocido");
            pipe_enviar(hCliente, err);
            json_liberar_str(err);
            cJSON_Delete(peticion);
            continue;
        }

        /* Reenviar la petición al servicio */
        pipe_enviar(hServicio, buf);

        /* Esperar respuesta del servicio y reenviarla al cliente */
        char resp[PIPE_BUF_SIZE];
        int m = pipe_recibir(hServicio, resp, sizeof(resp));
        if (m > 0) {
            pipe_enviar(hCliente, resp);
        }

        cJSON_Delete(peticion);
    }

    pipe_cerrar(hCliente);
    printf("[ctrllt] Cliente desconectado.\n");
    return 0;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("[ctrllt] Iniciando...\n");

    /* Conectar con los servicios (deben estar corriendo) */
    printf("[ctrllt] Conectando a gesfich...\n");
    h_gesfich = pipe_conectar_cliente(PIPE_GESFICH);

    printf("[ctrllt] Conectando a gesprog...\n");
    h_gesprog = pipe_conectar_cliente(PIPE_GESPROG);

    printf("[ctrllt] Conectando a ejecutor...\n");
    h_ejecutor = pipe_conectar_cliente(PIPE_EJECUTOR);

    /* Crear pipe de escucha para clientes */
    printf("[ctrllt] Escuchando clientes en '%s'...\n", PIPE_CLIENTE);

    while (1) {
        HANDLE hPipe = pipe_crear_servidor(PIPE_CLIENTE);
        if (hPipe == INVALID_HANDLE_VALUE) break;

        if (pipe_aceptar_cliente(hPipe) < 0) {
            pipe_cerrar(hPipe);
            continue;
        }

        printf("[ctrllt] Nuevo cliente conectado.\n");

        ClienteArgs *args = malloc(sizeof(ClienteArgs));
        args->hCliente = hPipe;
        CreateThread(NULL, 0, hilo_cliente, args, 0, NULL);
    }

    pipe_cerrar(h_gesfich);
    pipe_cerrar(h_gesprog);
    pipe_cerrar(h_ejecutor);
    return 0;
}
