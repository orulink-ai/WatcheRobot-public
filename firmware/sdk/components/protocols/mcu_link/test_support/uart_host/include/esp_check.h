#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_FALSE(condition, result, ...)                                                                    \
    do {                                                                                                               \
        if (!(condition))                                                                                              \
            return (result);                                                                                           \
    } while (0)
#define ESP_RETURN_ON_ERROR(expression, ...)                                                                           \
    do {                                                                                                               \
        esp_err_t e = (expression);                                                                                    \
        if (e != ESP_OK)                                                                                               \
            return e;                                                                                                  \
    } while (0)

