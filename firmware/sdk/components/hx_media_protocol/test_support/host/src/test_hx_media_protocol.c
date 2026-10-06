#include "hx_media_protocol.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

static int test_crc_reference_vector(void) {
    static const char text[] = "123456789";
    CHECK(hx_media_crc32(text, sizeof(text) - 1U) == UINT32_C(0xcbf43926));
    return 0;
}

static int test_encode_and_validate(void) {
    static const uint8_t payload[] = {0xff, 0xd8, 0x01, 0x02, 0xff, 0xd9};
    uint8_t header[HX_MEDIA_HEADER_SIZE];
    hx_media_header_t decoded;
    volatile unsigned source_width = HX_MEDIA_SOURCE_WIDTH;
    volatile unsigned source_height = HX_MEDIA_SOURCE_HEIGHT;
    volatile unsigned source_fps = HX_MEDIA_SOURCE_FPS;

    CHECK(hx_media_header_encode(header, HX_MEDIA_FLAG_KEY_FRAME | HX_MEDIA_FLAG_SOURCE_640X480, 42U, 1234U, payload,
                                 sizeof(payload)) == HX_MEDIA_OK);
    CHECK(hx_media_frame_validate(header, sizeof(header), payload, sizeof(payload), 1024U, &decoded) == HX_MEDIA_OK);
    CHECK(decoded.version == HX_MEDIA_VERSION);
    CHECK(decoded.flags == (HX_MEDIA_FLAG_KEY_FRAME | HX_MEDIA_FLAG_SOURCE_640X480));
    CHECK(source_width == 640U);
    CHECK(source_height == 480U);
    CHECK(source_fps == 24U);
    CHECK(decoded.sequence == 42U);
    CHECK(decoded.timestamp_ms == 1234U);
    CHECK(decoded.payload_crc32 == hx_media_crc32(payload, sizeof(payload)));
    CHECK(decoded.payload_crc32 == UINT32_C(0x3b901afe));
    return 0;
}

static int test_rejects_corruption_and_bad_bounds(void) {
    uint8_t payload[] = {1U, 2U, 3U, 4U};
    uint8_t header[HX_MEDIA_HEADER_SIZE];
    hx_media_header_t decoded;

    CHECK(hx_media_header_encode(header, HX_MEDIA_FLAG_SOURCE_640X480, 1U, 2U, payload, sizeof(payload)) ==
          HX_MEDIA_OK);
    CHECK(hx_media_frame_validate(header, sizeof(header) - 1U, payload, sizeof(payload), sizeof(payload), &decoded) ==
          HX_MEDIA_ERR_HEADER_SIZE);
    header[0] ^= 1U;
    CHECK(hx_media_frame_validate(header, sizeof(header), payload, sizeof(payload), sizeof(payload), &decoded) ==
          HX_MEDIA_ERR_MAGIC);
    header[0] ^= 1U;
    header[2] = HX_MEDIA_VERSION + 1U;
    CHECK(hx_media_frame_validate(header, sizeof(header), payload, sizeof(payload), sizeof(payload), &decoded) ==
          HX_MEDIA_ERR_VERSION);
    header[2] = HX_MEDIA_VERSION;
    CHECK(hx_media_frame_validate(header, sizeof(header), payload, sizeof(payload), sizeof(payload) - 1U, &decoded) ==
          HX_MEDIA_ERR_PAYLOAD_SIZE);
    payload[1] ^= 0x80U;
    CHECK(hx_media_frame_validate(header, sizeof(header), payload, sizeof(payload), sizeof(payload), &decoded) ==
          HX_MEDIA_ERR_CRC);
    return 0;
}

static int test_rejects_incompatible_source_profile(void) {
    static const uint8_t payload[] = {0xff, 0xd8, 0xff, 0xd9};
    uint8_t header[HX_MEDIA_HEADER_SIZE];
    hx_media_header_t decoded;

    CHECK(hx_media_header_encode(header, HX_MEDIA_FLAG_KEY_FRAME | HX_MEDIA_FLAG_SOURCE_320X240, 1U, 2U, payload,
                                 sizeof(payload)) == HX_MEDIA_OK);
    CHECK(hx_media_frame_validate(header, sizeof(header), payload, sizeof(payload), sizeof(payload), &decoded) ==
          HX_MEDIA_ERR_SOURCE_PROFILE);
    return 0;
}

static int test_sequence_accounting_and_wrap(void) {
    hx_media_sequence_tracker_t tracker;

    hx_media_sequence_tracker_reset(&tracker);
    hx_media_sequence_tracker_accept(&tracker, UINT32_MAX - 1U);
    hx_media_sequence_tracker_accept(&tracker, 1U);
    hx_media_sequence_tracker_accept(&tracker, 1U);
    hx_media_sequence_tracker_accept(&tracker, 0U);

    CHECK(tracker.accepted == 2U);
    CHECK(tracker.last_sequence == 1U);
    CHECK(tracker.gaps == 2U);
    CHECK(tracker.duplicates == 1U);
    CHECK(tracker.reordered == 1U);
    return 0;
}

int main(void) {
    CHECK(test_crc_reference_vector() == 0);
    CHECK(test_encode_and_validate() == 0);
    CHECK(test_rejects_corruption_and_bad_bounds() == 0);
    CHECK(test_rejects_incompatible_source_profile() == 0);
    CHECK(test_sequence_accounting_and_wrap() == 0);
    puts("hx_media_protocol_host_tests: PASS");
    return 0;
}

