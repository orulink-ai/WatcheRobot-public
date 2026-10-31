/**
 * @file mcu_link_test_support.h
 * @brief Helpers for protocol-core tests and byte-stream fixtures.
 */

#ifndef MCU_LINK_TEST_SUPPORT_H
#define MCU_LINK_TEST_SUPPORT_H

#include "mcu_frame.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t raw[MCU_FRAME_MAX_RAW_SIZE];
    size_t raw_len;
    uint8_t wire[MCU_FRAME_MAX_WIRE_SIZE];
    size_t wire_len;
} mcu_link_test_packet_t;

esp_err_t mcu_link_test_support_make_packet(const mcu_frame_header_t *header, const uint8_t *payload,
                                            mcu_link_test_packet_t *packet);
esp_err_t mcu_link_test_support_parse_packet(const uint8_t *wire, size_t wire_len, mcu_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_TEST_SUPPORT_H */
