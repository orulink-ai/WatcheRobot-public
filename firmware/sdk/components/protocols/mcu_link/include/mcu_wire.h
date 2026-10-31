/**
 * @file mcu_wire.h
 * @brief COBS + delimiter wire helpers for the coprocessor protocol.
 */

#ifndef MCU_WIRE_H
#define MCU_WIRE_H

#include "mcu_frame.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t mcu_wire_max_wire_size(size_t raw_len);
esp_err_t mcu_wire_encode_raw(const uint8_t *raw, size_t raw_len, uint8_t *wire, size_t wire_len, size_t *encoded_len);
esp_err_t mcu_wire_decode_raw(const uint8_t *wire, size_t wire_len, uint8_t *raw, size_t raw_len, size_t *decoded_len);
esp_err_t mcu_wire_encode_frame(const mcu_frame_header_t *header, const uint8_t *payload, uint8_t *wire,
                                size_t wire_len, size_t *encoded_len);
esp_err_t mcu_wire_decode_frame(const uint8_t *wire, size_t wire_len, mcu_frame_t *frame, size_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif /* MCU_WIRE_H */
