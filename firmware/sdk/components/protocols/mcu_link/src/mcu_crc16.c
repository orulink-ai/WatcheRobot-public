#include "mcu_crc16.h"

uint16_t mcu_crc16_ccitt_false_update(uint16_t seed, const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t crc = seed;

    if (bytes == NULL || len == 0) {
        return crc;
    }

    for (size_t i = 0; i < len; ++i) {
        crc ^= ((uint16_t)bytes[i] << 8);
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

uint16_t mcu_crc16_ccitt_false(const void *data, size_t len) {
    return mcu_crc16_ccitt_false_update(MCU_CRC16_CCITT_FALSE_INIT, data, len);
}
