#include "mcu_frame.h"

#include "mcu_crc16.h"

#include <string.h>

static void write_u16_le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint16_t read_u16_le(const uint8_t *src) {
    return (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

void mcu_frame_header_init(mcu_frame_header_t *header, uint8_t msg_class, uint8_t msg_id, uint8_t flags, uint32_t seq,
                           uint16_t payload_len) {
    if (header == NULL) {
        return;
    }

    header->magic0 = MCU_FRAME_MAGIC0;
    header->magic1 = MCU_FRAME_MAGIC1;
    header->proto_version = MCU_FRAME_PROTO_VERSION;
    header->msg_class = msg_class;
    header->msg_id = msg_id;
    header->flags = flags;
    header->seq = seq;
    header->payload_len = payload_len;
}

bool mcu_frame_header_is_valid(const mcu_frame_header_t *header) {
    if (header == NULL) {
        return false;
    }

    if (header->magic0 != MCU_FRAME_MAGIC0 || header->magic1 != MCU_FRAME_MAGIC1) {
        return false;
    }

    if (header->proto_version != MCU_FRAME_PROTO_VERSION) {
        return false;
    }

    return header->payload_len <= MCU_FRAME_MAX_PAYLOAD_SIZE;
}

uint16_t mcu_frame_compute_crc(const mcu_frame_header_t *header, const uint8_t *payload) {
    if (header == NULL || (header->payload_len > 0u && payload == NULL)) {
        return 0u;
    }

    uint8_t header_bytes[MCU_FRAME_PREFIX_SIZE];
    size_t offset = 0u;

    header_bytes[offset++] = header->magic0;
    header_bytes[offset++] = header->magic1;
    header_bytes[offset++] = header->proto_version;
    header_bytes[offset++] = header->msg_class;
    header_bytes[offset++] = header->msg_id;
    header_bytes[offset++] = header->flags;
    write_u32_le(&header_bytes[offset], header->seq);
    offset += sizeof(uint32_t);
    write_u16_le(&header_bytes[offset], header->payload_len);
    offset += sizeof(uint16_t);

    uint16_t crc = mcu_crc16_ccitt_false(header_bytes, offset);
    if (header->payload_len > 0u) {
        crc = mcu_crc16_ccitt_false_update(crc, payload, header->payload_len);
    }

    return crc;
}

esp_err_t mcu_frame_pack(const mcu_frame_header_t *header, const uint8_t *payload, uint8_t *buffer, size_t buffer_len,
                         size_t *encoded_len) {
    if (header == NULL || buffer == NULL || encoded_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mcu_frame_header_is_valid(header)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (header->payload_len > 0u && payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t raw_len = (size_t)MCU_FRAME_PREFIX_SIZE + header->payload_len + MCU_FRAME_CRC_SIZE;
    if (buffer_len < raw_len) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t *cursor = buffer;
    *cursor++ = header->magic0;
    *cursor++ = header->magic1;
    *cursor++ = header->proto_version;
    *cursor++ = header->msg_class;
    *cursor++ = header->msg_id;
    *cursor++ = header->flags;
    write_u32_le(cursor, header->seq);
    cursor += sizeof(uint32_t);
    write_u16_le(cursor, header->payload_len);
    cursor += sizeof(uint16_t);

    if (header->payload_len > 0u) {
        memcpy(cursor, payload, header->payload_len);
        cursor += header->payload_len;
    }

    const uint16_t crc = mcu_frame_compute_crc(header, payload);
    write_u16_le(cursor, crc);
    cursor += sizeof(uint16_t);

    *encoded_len = (size_t)(cursor - buffer);
    return ESP_OK;
}

esp_err_t mcu_frame_unpack(const uint8_t *buffer, size_t buffer_len, mcu_frame_t *frame, size_t *payload_len) {
    if (buffer == NULL || frame == NULL || payload_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_len < (size_t)(MCU_FRAME_PREFIX_SIZE + MCU_FRAME_CRC_SIZE)) {
        return ESP_ERR_INVALID_SIZE;
    }

    mcu_frame_header_t header = {
        .magic0 = buffer[0],
        .magic1 = buffer[1],
        .proto_version = buffer[2],
        .msg_class = buffer[3],
        .msg_id = buffer[4],
        .flags = buffer[5],
        .seq = read_u32_le(&buffer[6]),
        .payload_len = read_u16_le(&buffer[10]),
    };

    if (!mcu_frame_header_is_valid(&header)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((size_t)MCU_FRAME_PREFIX_SIZE + header.payload_len + MCU_FRAME_CRC_SIZE != buffer_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint16_t expected_crc = read_u16_le(&buffer[MCU_FRAME_PREFIX_SIZE + header.payload_len]);
    const uint16_t crc = mcu_frame_compute_crc(&header, &buffer[MCU_FRAME_PREFIX_SIZE]);
    if (crc != expected_crc) {
        return ESP_ERR_INVALID_CRC;
    }

    frame->header = header;
    if (header.payload_len > 0u) {
        memcpy(frame->payload, &buffer[MCU_FRAME_PREFIX_SIZE], header.payload_len);
    }
    frame->crc16 = expected_crc;
    *payload_len = header.payload_len;
    return ESP_OK;
}
