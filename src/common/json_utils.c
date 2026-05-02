#include "json_utils.h"
#include <stdlib.h>

char *json_construir_peticion(const char *servicio,
                              const char *operacion,
                              cJSON      *params) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "servicio",  servicio);
    cJSON_AddStringToObject(root, "operacion", operacion);
    if (params) {
        cJSON_AddItemToObject(root, "parametros", params);
    }
    char *str = cJSON_PrintUnformatted(root);
    /* Desacoplar params antes de borrar root para no liberar params */
    if (params) {
        cJSON_DetachItemFromObject(root, "parametros");
    }
    cJSON_Delete(root);
    return str;
}

char *json_construir_respuesta(const char *estado,
                               cJSON      *datos,
                               const char *msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "estado", estado);
    if (datos) {
        cJSON_AddItemToObject(root, "datos", datos);
    }
    if (msg) {
        cJSON_AddStringToObject(root, "mensaje", msg);
    }
    char *str = cJSON_PrintUnformatted(root);
    if (datos) {
        cJSON_DetachItemFromObject(root, "datos");
    }
    cJSON_Delete(root);
    return str;
}

cJSON *json_parsear(const char *texto) {
    return cJSON_Parse(texto);
}

const char *json_get_str(const cJSON *obj, const char *campo) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, campo);
    if (cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

void json_liberar_str(char *str) {
    free(str);
}
