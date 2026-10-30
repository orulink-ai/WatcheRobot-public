/**
 * @file mcu_cobs.h
 * @brief COBS helpers for UART wire framing.
 */

#ifndef MCU_COBS_H
#define MCU_COBS_H

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t mcu_cobs_max_encoded_size(size_t input_len);
esp_err_t mcu_cobs_encode(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_len,
                          size_t *encoded_len);
esp_err_t mcu_cobs_decode(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_len,
                          size_t *decoded_len);

#ifdef __cplusplus
}
#endif

#endif /* MCU_COBS_H */
