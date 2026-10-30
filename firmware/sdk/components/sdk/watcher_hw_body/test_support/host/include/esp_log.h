#pragma once
#include "esp_err.h"
/* Evaluate arguments: examples intentionally log the result of API calls. */
static inline const char *esp_err_to_name(esp_err_t error) {
    (void)error;
    return "mock";
}
static inline void host_log(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}
#define ESP_LOGI(...) host_log(__VA_ARGS__)
#define ESP_LOGW(...) host_log(__VA_ARGS__)
#define ESP_LOGE(...) host_log(__VA_ARGS__)
