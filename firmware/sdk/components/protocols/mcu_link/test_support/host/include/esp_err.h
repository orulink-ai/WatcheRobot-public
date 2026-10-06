#ifndef MCU_LINK_HOST_TEST_ESP_ERR_H
#define MCU_LINK_HOST_TEST_ESP_ERR_H

#include <stdint.h>

typedef int32_t esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_ARG -3
#define ESP_ERR_INVALID_SIZE -4
#define ESP_ERR_NOT_FOUND -5
#define ESP_ERR_NOT_SUPPORTED -6
#define ESP_ERR_INVALID_STATE -7
#define ESP_ERR_INVALID_CRC -8
#define ESP_ERR_TIMEOUT -9

#endif /* MCU_LINK_HOST_TEST_ESP_ERR_H */

