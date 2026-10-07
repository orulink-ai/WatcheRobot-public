#pragma once
#include <stdbool.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_INVALID_STATE -3
#define ESP_ERR_INVALID_RESPONSE -4
#define ESP_ERR_TIMEOUT -5
static inline const char *esp_err_to_name(int error) {
    (void)error;
    return "error";
}

