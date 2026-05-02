#ifndef PIPE_UTILS_H
#define PIPE_UTILS_H

#include <windows.h>
#include <stddef.h>

/* Prefijo estándar de Named Pipes en Windows */
#define PIPE_PREFIX     "\\\\.\\pipe\\"
#define PIPE_BUF_SIZE   65536   /* 64 KB por mensaje */
#define PIPE_TIMEOUT_MS 5000

/*
 * Crea una Named Pipe como servidor (lado que escucha).
 * nombre: solo el nombre corto, ej. "ctrllt_cliente"
 * Retorna el HANDLE o INVALID_HANDLE_VALUE en error.
 */
HANDLE pipe_crear_servidor(const char *nombre);

/*
 * Espera a que un cliente se conecte a la pipe de servidor.
 * Bloquea hasta que llega una conexión.
 * Retorna 0 en éxito, -1 en error.
 */
int pipe_aceptar_cliente(HANDLE hPipe);

/*
 * Conecta como cliente a una Named Pipe existente.
 * nombre: solo el nombre corto, ej. "ctrllt_cliente"
 * Retorna el HANDLE o INVALID_HANDLE_VALUE en error.
 */
HANDLE pipe_conectar_cliente(const char *nombre);

/*
 * Envía un mensaje JSON por la pipe.
 * El mensaje se envía como texto terminado en '\n'.
 * Retorna bytes escritos o -1 en error.
 */
int pipe_enviar(HANDLE hPipe, const char *mensaje);

/*
 * Recibe un mensaje JSON de la pipe.
 * Lee hasta '\n' o hasta llenar el buffer.
 * buf debe tener al menos PIPE_BUF_SIZE bytes.
 * Retorna bytes leídos o -1 en error / conexión cerrada.
 */
int pipe_recibir(HANDLE hPipe, char *buf, size_t buf_size);

/*
 * Cierra el handle de la pipe limpiamente.
 */
void pipe_cerrar(HANDLE hPipe);

#endif /* PIPE_UTILS_H */
