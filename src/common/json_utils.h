#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "cjson/cJSON.h"

/*
 * Construye un mensaje de petición JSON.
 * servicio:  "gesfich" | "gesprog" | "ejecutor"
 * operacion: "CREAR" | "LEER" | "GUARDAR" | "EJECUTAR" | etc.
 * params:    objeto cJSON con los parámetros (puede ser NULL)
 *
 * El llamador debe liberar la cadena retornada con json_liberar_str().
 */
char *json_construir_peticion(const char *servicio,
                              const char *operacion,
                              cJSON      *params);

/*
 * Construye una respuesta JSON estándar.
 * estado: "OK" o "ERROR"
 * datos:  objeto cJSON con los datos (puede ser NULL)
 * msg:    mensaje descriptivo (puede ser NULL)
 *
 * El llamador debe liberar con json_liberar_str().
 */
char *json_construir_respuesta(const char *estado,
                               cJSON      *datos,
                               const char *msg);

/*
 * Parsea una cadena JSON.
 * El llamador debe liberar el resultado con cJSON_Delete().
 * Retorna NULL si el JSON es inválido.
 */
cJSON *json_parsear(const char *texto);

/*
 * Extrae el valor de un campo string de un objeto JSON.
 * Retorna NULL si el campo no existe o no es string.
 */
const char *json_get_str(const cJSON *obj, const char *campo);

/*
 * Libera una cadena generada por cJSON_PrintUnformatted.
 */
void json_liberar_str(char *str);

#endif /* JSON_UTILS_H */
