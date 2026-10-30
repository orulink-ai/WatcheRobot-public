/**
 * @file mcu_crc16.h
 * @brief CRC16 helpers for the mcu_link board-internal protocol.
 */

#ifndef MCU_CRC16_H
#define MCU_CRC16_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCU_CRC16_CCITT_FALSE_INIT 0xFFFFu

uint16_t mcu_crc16_ccitt_false_update(uint16_t seed, const void *data, size_t len);
uint16_t mcu_crc16_ccitt_false(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MCU_CRC16_H */
