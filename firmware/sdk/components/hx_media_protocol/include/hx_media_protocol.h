#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Watcher media metadata carried in the 16-byte user header supported by the
 * Himax SPI PTL write_simple_ex API. Multi-byte fields are little-endian.
 * The PTL base header remains responsible for media type and payload length.
 */
#define HX_MEDIA_HEADER_SIZE 16U
#define HX_MEDIA_MAGIC_0 0x57U /* 'W' */
#define HX_MEDIA_MAGIC_1 0x52U /* 'R' */
#define HX_MEDIA_VERSION 1U

enum {
    HX_MEDIA_FLAG_KEY_FRAME = 1U << 0,
    HX_MEDIA_FLAG_DISCONTINUITY = 1U << 1,
    HX_MEDIA_FLAG_SOURCE_PROFILE_MASK = 3U << 4,
    HX_MEDIA_FLAG_SOURCE_320X240 = 1U << 4,
    HX_MEDIA_FLAG_SOURCE_640X480 = 2U << 4,
};

#define HX_MEDIA_SOURCE_WIDTH 640U
#define HX_MEDIA_SOURCE_HEIGHT 480U
#define HX_MEDIA_SOURCE_FPS 24U

typedef struct {
    uint8_t version;
    uint8_t flags;
    uint32_t sequence;
    uint32_t timestamp_ms;
    uint32_t payload_crc32;
} hx_media_header_t;

typedef enum {
    HX_MEDIA_OK = 0,
    HX_MEDIA_ERR_ARGUMENT,
    HX_MEDIA_ERR_HEADER_SIZE,
    HX_MEDIA_ERR_MAGIC,
    HX_MEDIA_ERR_VERSION,
    HX_MEDIA_ERR_SOURCE_PROFILE,
    HX_MEDIA_ERR_PAYLOAD_SIZE,
    HX_MEDIA_ERR_CRC,
} hx_media_result_t;

typedef struct {
    bool initialized;
    uint32_t last_sequence;
    uint32_t accepted;
    uint32_t gaps;
    uint32_t duplicates;
    uint32_t reordered;
} hx_media_sequence_tracker_t;

uint32_t hx_media_crc32(const void *data, size_t size);

hx_media_result_t hx_media_header_encode(uint8_t out[HX_MEDIA_HEADER_SIZE], uint8_t flags, uint32_t sequence,
                                         uint32_t timestamp_ms, const void *payload, size_t payload_size);

hx_media_result_t hx_media_frame_validate(const uint8_t *header, size_t header_size, const void *payload,
                                          size_t payload_size, size_t max_payload_size, hx_media_header_t *decoded);

void hx_media_sequence_tracker_reset(hx_media_sequence_tracker_t *tracker);
void hx_media_sequence_tracker_accept(hx_media_sequence_tracker_t *tracker, uint32_t sequence);

const char *hx_media_result_name(hx_media_result_t result);

#ifdef __cplusplus
}
#endif

