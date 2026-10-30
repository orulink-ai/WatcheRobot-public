#include "mcu_wire.h"

#include "mcu_cobs.h"

size_t mcu_wire_max_wire_size(size_t raw_len) {
    return mcu_cobs_max_encoded_size(raw_len) + 1u;
}

esp_err_t mcu_wire_encode_raw(const uint8_t *raw, size_t raw_len, uint8_t *wire, size_t wire_len, size_t *encoded_len) {
    if (wire == NULL || encoded_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *encoded_len = 0u;

    size_t cobs_len = 0u;
    esp_err_t err = mcu_cobs_encode(raw, raw_len, wire, wire_len, &cobs_len);
    if (err != ESP_OK) {
        return err;
    }

    if (cobs_len + 1u > wire_len) {
        return ESP_ERR_NO_MEM;
    }

    wire[cobs_len] = 0u;
    *encoded_len = cobs_len + 1u;
    return ESP_OK;
}

esp_err_t mcu_wire_decode_raw(const uint8_t *wire, size_t wire_len, uint8_t *raw, size_t raw_len, size_t *decoded_len) {
    if (wire == NULL || raw == NULL || decoded_len == NULL || wire_len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    if (wire[wire_len - 1u] != 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    return mcu_cobs_decode(wire, wire_len - 1u, raw, raw_len, decoded_len);
}

esp_err_t mcu_wire_encode_frame(const mcu_frame_header_t *header, const uint8_t *payload, uint8_t *wire,
                                size_t wire_len, size_t *encoded_len) {
    if (header == NULL || wire == NULL || encoded_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[MCU_FRAME_MAX_RAW_SIZE];
    size_t raw_len = 0u;
    esp_err_t err = mcu_frame_pack(header, payload, raw, sizeof(raw), &raw_len);
    if (err != ESP_OK) {
        return err;
    }

    return mcu_wire_encode_raw(raw, raw_len, wire, wire_len, encoded_len);
}

esp_err_t mcu_wire_decode_frame(const uint8_t *wire, size_t wire_len, mcu_frame_t *frame, size_t *payload_len) {
    if (wire == NULL || frame == NULL || payload_len == NULL || wire_len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[MCU_FRAME_MAX_RAW_SIZE];
    size_t raw_len = 0u;
    esp_err_t err = mcu_wire_decode_raw(wire, wire_len, raw, sizeof(raw), &raw_len);
    if (err != ESP_OK) {
        return err;
    }

    return mcu_frame_unpack(raw, raw_len, frame, payload_len);
}
