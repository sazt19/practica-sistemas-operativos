#include "pipe_utils.h"
#include <stdio.h>
#include <string.h>

/* Construye el nombre completo \\.\pipe\<nombre> */
static void nombre_completo(const char *nombre, char *out, size_t out_size) {
    snprintf(out, out_size, "%s%s", PIPE_PREFIX, nombre);
}

/* ── Servidor ─────────────────────────────────────────────────────────────── */

HANDLE pipe_crear_servidor(const char *nombre) {
    char ruta[256];
    nombre_completo(nombre, ruta, sizeof(ruta));

    HANDLE h = CreateNamedPipeA(
        ruta,
        PIPE_ACCESS_DUPLEX,                         /* full-duplex */
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        PIPE_BUF_SIZE,
        PIPE_BUF_SIZE,
        PIPE_TIMEOUT_MS,
        NULL                                        /* sin seguridad especial */
    );

    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[pipe] Error creando servidor '%s': %lu\n",
                ruta, GetLastError());
    } else {
        fprintf(stderr, "[pipe] Servidor escuchando en '%s'\n", ruta);
    }
    return h;
}

int pipe_aceptar_cliente(HANDLE hPipe) {
    if (ConnectNamedPipe(hPipe, NULL)) {
        return 0;
    }
    /* ERROR_PIPE_CONNECTED significa que el cliente ya conectó antes del llamado */
    if (GetLastError() == ERROR_PIPE_CONNECTED) {
        return 0;
    }
    fprintf(stderr, "[pipe] Error aceptando cliente: %lu\n", GetLastError());
    return -1;
}

/* ── Cliente ──────────────────────────────────────────────────────────────── */

HANDLE pipe_conectar_cliente(const char *nombre) {
    char ruta[256];
    nombre_completo(nombre, ruta, sizeof(ruta));

    /* Reintentar si el servidor todavía no está listo */
    for (int intento = 0; intento < 10; intento++) {
        HANDLE h = CreateFileA(
            ruta,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (h != INVALID_HANDLE_VALUE) {
            /* Cambiar a modo mensaje para leer correctamente */
            DWORD modo = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(h, &modo, NULL, NULL);
            fprintf(stderr, "[pipe] Conectado a '%s'\n", ruta);
            return h;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            /* El servidor no existe aún, esperar */
            fprintf(stderr, "[pipe] Esperando servidor '%s' (intento %d)...\n",
                    ruta, intento + 1);
            Sleep(500);
            continue;
        }

        /* Pipe ocupada, esperar con WaitNamedPipe */
        WaitNamedPipeA(ruta, PIPE_TIMEOUT_MS);
    }

    fprintf(stderr, "[pipe] No se pudo conectar a '%s'\n", ruta);
    return INVALID_HANDLE_VALUE;
}

/* ── Envío / Recepción ────────────────────────────────────────────────────── */

int pipe_enviar(HANDLE hPipe, const char *mensaje) {
    /* Añadir '\n' como delimitador de mensaje */
    char buf[PIPE_BUF_SIZE];
    int len = snprintf(buf, sizeof(buf), "%s\n", mensaje);

    DWORD escritos = 0;
    BOOL ok = WriteFile(hPipe, buf, (DWORD)len, &escritos, NULL);
    if (!ok) {
        fprintf(stderr, "[pipe] Error enviando: %lu\n", GetLastError());
        return -1;
    }
    return (int)escritos;
}

int pipe_recibir(HANDLE hPipe, char *buf, size_t buf_size) {
    DWORD leidos = 0;
    BOOL ok = ReadFile(hPipe, buf, (DWORD)(buf_size - 1), &leidos, NULL);

    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
            return -1;  /* cliente desconectado */
        }
        fprintf(stderr, "[pipe] Error recibiendo: %lu\n", err);
        return -1;
    }

    buf[leidos] = '\0';
    /* Quitar el '\n' final si existe */
    if (leidos > 0 && buf[leidos - 1] == '\n') {
        buf[leidos - 1] = '\0';
        leidos--;
    }
    return (int)leidos;
}

/* ── Cierre ───────────────────────────────────────────────────────────────── */

void pipe_cerrar(HANDLE hPipe) {
    if (hPipe != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}
