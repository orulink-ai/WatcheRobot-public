#pragma once

#include <stdbool.h>

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

bool sscma_client_parse_sensor_payload(const cJSON *payload, int *id, int *type, int *state, int *opt_id,
                                       char **opt_detail);

#ifdef __cplusplus
}
#endif

