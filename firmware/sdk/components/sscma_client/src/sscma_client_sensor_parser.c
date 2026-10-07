#include "sscma_client_sensor_parser.h"

#include <stdlib.h>
#include <string.h>

static bool sscma_client_read_int(const cJSON *object, const char *name, int *value) {
    const cJSON *field = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(field) || value == NULL) {
        return false;
    }
    *value = field->valueint;
    return true;
}

static bool sscma_client_read_optional_int(const cJSON *object, const char *name, int *value) {
    const cJSON *field = cJSON_GetObjectItemCaseSensitive(object, name);

    if (value == NULL) {
        return false;
    }
    if (field == NULL) {
        *value = 0;
        return true;
    }
    if (!cJSON_IsNumber(field)) {
        return false;
    }
    *value = field->valueint;
    return true;
}

bool sscma_client_parse_sensor_payload(const cJSON *payload, int *id, int *type, int *state, int *opt_id,
                                       char **opt_detail) {
    const cJSON *data;
    const cJSON *sensor;
    const cJSON *detail;
    char *detail_copy;
    int parsed_id;
    int parsed_type;
    int parsed_state;
    int parsed_opt_id;

    if (!cJSON_IsObject(payload) || id == NULL || type == NULL || state == NULL || opt_id == NULL ||
        opt_detail == NULL) {
        return false;
    }

    data = cJSON_GetObjectItemCaseSensitive(payload, "data");
    if (!cJSON_IsObject(data)) {
        return false;
    }

    sensor = cJSON_GetObjectItemCaseSensitive(data, "sensor");
    if (!cJSON_IsObject(sensor)) {
        sensor = data;
    }

    detail = cJSON_GetObjectItemCaseSensitive(sensor, "opt_detail");
    if (!cJSON_IsString(detail) || detail->valuestring == NULL || !sscma_client_read_int(sensor, "id", &parsed_id) ||
        !sscma_client_read_optional_int(sensor, "type", &parsed_type) ||
        !sscma_client_read_optional_int(sensor, "state", &parsed_state) ||
        !sscma_client_read_int(sensor, "opt_id", &parsed_opt_id)) {
        return false;
    }

    detail_copy = malloc(strlen(detail->valuestring) + 1u);
    if (detail_copy == NULL) {
        return false;
    }
    strcpy(detail_copy, detail->valuestring);
    *id = parsed_id;
    *type = parsed_type;
    *state = parsed_state;
    *opt_id = parsed_opt_id;
    *opt_detail = detail_copy;
    return true;
}

