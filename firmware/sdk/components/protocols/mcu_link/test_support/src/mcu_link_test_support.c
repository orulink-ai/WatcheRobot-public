#include "mcu_link_test_support.h"

#include "mcu_wire.h"

esp_err_t mcu_link_test_support_make_packet(const mcu_frame_header_t *header, const uint8_t *payload,
                                            mcu_link_test_packet_t *packet) {
    if (header == NULL || packet == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t raw_len = 0u;
    esp_err_t err = mcu_frame_pack(header, payload, packet->raw, sizeof(packet->raw), &raw_len);
    if (err != ESP_OK) {
        return err;
    }

    size_t encoded_len = 0u;
    err = mcu_wire_encode_raw(packet->raw, raw_len, packet->wire, sizeof(packet->wire), &encoded_len);
    if (err != ESP_OK) {
        return err;
    }

    packet->raw_len = raw_len;
    packet->wire_len = encoded_len;
    return ESP_OK;
}

esp_err_t mcu_link_test_support_parse_packet(const uint8_t *wire, size_t wire_len, mcu_frame_t *frame) {
    if (wire == NULL || frame == NULL || wire_len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[MCU_FRAME_MAX_RAW_SIZE];
    size_t raw_len = 0u;

    esp_err_t err = mcu_wire_decode_raw(wire, wire_len, raw, sizeof(raw), &raw_len);
    if (err != ESP_OK) {
        return err;
    }

    size_t payload_len = 0u;
    err = mcu_frame_unpack(raw, raw_len, frame, &payload_len);
    if (err != ESP_OK) {
        return err;
    }

    frame->header.payload_len = (uint16_t)payload_len;
    return ESP_OK;
}
