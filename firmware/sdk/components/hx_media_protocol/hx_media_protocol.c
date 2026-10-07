#include "hx_media_protocol.h"

#include <string.h>

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_le32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

uint32_t hx_media_crc32(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_C(0xffffffff);

    if (bytes == NULL && size != 0U) {
        return 0U;
    }
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t) - (int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

hx_media_result_t hx_media_header_encode(uint8_t out[HX_MEDIA_HEADER_SIZE], uint8_t flags, uint32_t sequence,
                                         uint32_t timestamp_ms, const void *payload, size_t payload_size) {
    if (out == NULL || (payload == NULL && payload_size != 0U)) {
        return HX_MEDIA_ERR_ARGUMENT;
    }

    out[0] = HX_MEDIA_MAGIC_0;
    out[1] = HX_MEDIA_MAGIC_1;
    out[2] = HX_MEDIA_VERSION;
    out[3] = flags;
    write_le32(&out[4], sequence);
    write_le32(&out[8], timestamp_ms);
    write_le32(&out[12], hx_media_crc32(payload, payload_size));
    return HX_MEDIA_OK;
}

hx_media_result_t hx_media_frame_validate(const uint8_t *header, size_t header_size, const void *payload,
                                          size_t payload_size, size_t max_payload_size, hx_media_header_t *decoded) {
    hx_media_header_t value;

    if (header == NULL || decoded == NULL || (payload == NULL && payload_size != 0U)) {
        return HX_MEDIA_ERR_ARGUMENT;
    }
    if (header_size != HX_MEDIA_HEADER_SIZE) {
        return HX_MEDIA_ERR_HEADER_SIZE;
    }
    if (header[0] != HX_MEDIA_MAGIC_0 || header[1] != HX_MEDIA_MAGIC_1) {
        return HX_MEDIA_ERR_MAGIC;
    }
    if (header[2] != HX_MEDIA_VERSION) {
        return HX_MEDIA_ERR_VERSION;
    }
    if ((header[3] & HX_MEDIA_FLAG_SOURCE_PROFILE_MASK) != HX_MEDIA_FLAG_SOURCE_640X480) {
        return HX_MEDIA_ERR_SOURCE_PROFILE;
    }
    if (payload_size > max_payload_size) {
        return HX_MEDIA_ERR_PAYLOAD_SIZE;
    }

    value.version = header[2];
    value.flags = header[3];
    value.sequence = read_le32(&header[4]);
    value.timestamp_ms = read_le32(&header[8]);
    value.payload_crc32 = read_le32(&header[12]);
    if (value.payload_crc32 != hx_media_crc32(payload, payload_size)) {
        return HX_MEDIA_ERR_CRC;
    }

    *decoded = value;
    return HX_MEDIA_OK;
}

void hx_media_sequence_tracker_reset(hx_media_sequence_tracker_t *tracker) {
    if (tracker != NULL) {
        memset(tracker, 0, sizeof(*tracker));
    }
}

void hx_media_sequence_tracker_accept(hx_media_sequence_tracker_t *tracker, uint32_t sequence) {
    uint32_t delta;

    if (tracker == NULL) {
        return;
    }
    if (!tracker->initialized) {
        tracker->initialized = true;
        tracker->last_sequence = sequence;
        tracker->accepted = 1U;
        return;
    }

    delta = sequence - tracker->last_sequence;
    if (delta == 0U) {
        ++tracker->duplicates;
        return;
    }
    if (delta < UINT32_C(0x80000000)) {
        tracker->gaps += delta - 1U;
        tracker->last_sequence = sequence;
        ++tracker->accepted;
        return;
    }
    ++tracker->reordered;
}

const char *hx_media_result_name(hx_media_result_t result) {
    switch (result) {
    case HX_MEDIA_OK:
        return "ok";
    case HX_MEDIA_ERR_ARGUMENT:
        return "argument";
    case HX_MEDIA_ERR_HEADER_SIZE:
        return "header_size";
    case HX_MEDIA_ERR_MAGIC:
        return "magic";
    case HX_MEDIA_ERR_VERSION:
        return "version";
    case HX_MEDIA_ERR_SOURCE_PROFILE:
        return "source_profile";
    case HX_MEDIA_ERR_PAYLOAD_SIZE:
        return "payload_size";
    case HX_MEDIA_ERR_CRC:
        return "crc";
    default:
        return "unknown";
    }
}

