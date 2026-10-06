#include "hx_ptl_protocol.h"
#include "hx_ptl_transport.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                       \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

typedef struct {
    const uint8_t *source;
    size_t source_size;
    size_t source_offset;
    size_t read_sizes[16];
    size_t read_count;
    uint32_t delays[8];
    size_t delay_count;
} transport_fixture_t;

static int fixture_read(void *context, uint8_t *data, size_t size) {
    transport_fixture_t *fixture = context;

    if (fixture->source_offset + size > fixture->source_size ||
        fixture->read_count >= sizeof(fixture->read_sizes) / sizeof(fixture->read_sizes[0])) {
        return -1;
    }
    fixture->read_sizes[fixture->read_count++] = size;
    memcpy(data, &fixture->source[fixture->source_offset], size);
    fixture->source_offset += size;
    return 0;
}
static void fixture_delay(void *context, uint32_t delay_ms) {
    transport_fixture_t *fixture = context;
    fixture->delays[fixture->delay_count++] = delay_ms;
}

static void write_le32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static int test_valid_packet(void) {
    static const uint8_t jpeg[] = {0xff, 0xd8, 1U, 2U, 0xff, 0xd9};
    static const uint8_t wire_payload[] = {
        0xff, 0xd8, 1U, 2U, 0xff, 0xd9, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
    };
    uint8_t base[HX_PTL_BASE_HEADER_SIZE] = {HX_PTL_MAGIC_0, HX_PTL_MAGIC_1, HX_PTL_DATA_TYPE_JPEG, 0U, 0U, 0U, 0U};
    uint8_t body[HX_MEDIA_HEADER_SIZE + sizeof(wire_payload)];
    hx_ptl_frame_t frame;

    write_le32(&base[3], sizeof(body));
    /* The Himax JPEG encoder includes its alignment bytes in the submitted
     * media payload. Authenticate that complete payload before trimming the
     * bytes after EOI for the browser. */
    CHECK(hx_media_header_encode(body, HX_MEDIA_FLAG_KEY_FRAME | HX_MEDIA_FLAG_SOURCE_640X480, 9U, 77U, wire_payload,
                                 sizeof(wire_payload)) == HX_MEDIA_OK);
    memcpy(&body[HX_MEDIA_HEADER_SIZE], wire_payload, sizeof(wire_payload));
    CHECK(hx_ptl_validate_jpeg_packet(base, sizeof(base), body, sizeof(body), 1024U, &frame) == HX_PTL_OK);
    CHECK(frame.media.sequence == 9U);
    CHECK(frame.media.timestamp_ms == 77U);
    CHECK(frame.ptl.body_size == sizeof(body));
    CHECK(frame.ptl.payload_size == sizeof(wire_payload));
    CHECK(frame.payload_size == sizeof(jpeg));
    CHECK(memcmp(frame.payload, jpeg, sizeof(jpeg)) == 0);
    return 0;
}

static int test_rejects_invalid_packets(void) {
    uint8_t base[HX_PTL_BASE_HEADER_SIZE] = {HX_PTL_MAGIC_0, HX_PTL_MAGIC_1, HX_PTL_DATA_TYPE_JPEG, 0U, 0U, 0U, 0U};
    uint8_t body[HX_MEDIA_HEADER_SIZE + 1U] = {0};
    hx_ptl_base_header_t parsed;
    hx_ptl_frame_t frame;

    write_le32(&base[3], sizeof(body));
    CHECK(hx_ptl_parse_base_header(base, sizeof(base) - 1U, 100U, &parsed) == HX_PTL_ERR_BASE_HEADER_SIZE);
    memset(base, 0, sizeof(base));
    CHECK(hx_ptl_parse_base_header(base, sizeof(base), 100U, &parsed) == HX_PTL_ERR_EMPTY);
    base[0] = HX_PTL_MAGIC_0;
    base[1] = HX_PTL_MAGIC_1;
    base[2] = HX_PTL_DATA_TYPE_JPEG;
    write_le32(&base[3], sizeof(body));
    base[0] = 0U;
    CHECK(hx_ptl_parse_base_header(base, sizeof(base), 100U, &parsed) == HX_PTL_ERR_MAGIC);
    base[0] = HX_PTL_MAGIC_0;
    base[2] = 2U;
    CHECK(hx_ptl_parse_base_header(base, sizeof(base), 100U, &parsed) == HX_PTL_ERR_MEDIA_TYPE);
    base[2] = HX_PTL_DATA_TYPE_JPEG;
    write_le32(&base[3], HX_MEDIA_HEADER_SIZE + 101U);
    CHECK(hx_ptl_parse_base_header(base, sizeof(base), 100U, &parsed) == HX_PTL_ERR_BODY_SIZE);
    write_le32(&base[3], HX_MEDIA_HEADER_SIZE - 1U);
    CHECK(hx_ptl_validate_jpeg_packet(base, sizeof(base), body, sizeof(body) - 1U, 100U, &frame) ==
          HX_PTL_ERR_BODY_SIZE);
    write_le32(&base[3], sizeof(body));
    CHECK(hx_ptl_validate_jpeg_packet(base, sizeof(base), body, sizeof(body), 100U, &frame) == HX_PTL_ERR_JPEG);
    write_le32(&base[3], HX_MEDIA_HEADER_SIZE + 101U);
    CHECK(hx_ptl_parse_base_header(base, sizeof(base), 100U, &parsed) == HX_PTL_ERR_BODY_SIZE);
    return 0;
}

static int test_transport_matches_himax_master_sequence(void) {
    static const uint8_t header_source[] = {
        0x11U, 0x22U, HX_PTL_MAGIC_0, 0x33U, HX_PTL_MAGIC_1, HX_PTL_DATA_TYPE_JPEG, 0x10U, 0x00U, 0x00U, 0x00U,
    };
    uint8_t header[HX_PTL_BASE_HEADER_SIZE] = {0};
    transport_fixture_t fixture = {
        .source = header_source,
        .source_size = sizeof(header_source),
    };
    hx_ptl_transport_io_t io = {
        .read = fixture_read,
        .delay = fixture_delay,
        .context = &fixture,
    };

    CHECK(hx_ptl_transport_read_base_header(&io, header) == HX_PTL_TRANSPORT_OK);
    CHECK(memcmp(header, (uint8_t[]){HX_PTL_MAGIC_0, HX_PTL_MAGIC_1, HX_PTL_DATA_TYPE_JPEG, 0x10U, 0U, 0U, 0U},
                 sizeof(header)) == 0);
    CHECK(fixture.read_count == 6U);
    CHECK(fixture.read_sizes[0] == 1U && fixture.read_sizes[1] == 1U && fixture.read_sizes[2] == 1U &&
          fixture.read_sizes[3] == 1U && fixture.read_sizes[4] == 1U && fixture.read_sizes[5] == 5U);
    return 0;
}

static int test_transport_primes_and_chunks_body(void) {
    enum { BODY_SIZE = HX_PTL_TRANSPORT_BODY_CHUNK_SIZE + 3U };
    uint8_t source[BODY_SIZE];
    uint8_t body[BODY_SIZE];
    transport_fixture_t fixture = {
        .source = source,
        .source_size = sizeof(source),
    };
    hx_ptl_transport_io_t io = {
        .read = fixture_read,
        .delay = fixture_delay,
        .context = &fixture,
    };

    memset(source, 0x5a, sizeof(source));
    CHECK(hx_ptl_transport_read_body(&io, body, sizeof(body)) == HX_PTL_TRANSPORT_OK);
    CHECK(fixture.read_count == 2U);
    CHECK(fixture.read_sizes[0] == HX_PTL_TRANSPORT_BODY_CHUNK_SIZE);
    CHECK(fixture.read_sizes[1] == 3U);
    CHECK(fixture.delay_count == 2U);
    CHECK(fixture.delays[0] == HX_PTL_TRANSPORT_BODY_PRE_DELAY_MS);
    CHECK(fixture.delays[1] == HX_PTL_TRANSPORT_BODY_INTER_CHUNK_DELAY_MS);
    return 0;
}

int main(void) {
    CHECK(test_valid_packet() == 0);
    CHECK(test_rejects_invalid_packets() == 0);
    CHECK(test_transport_matches_himax_master_sequence() == 0);
    CHECK(test_transport_primes_and_chunks_body() == 0);
    puts("hx_ptl_protocol_host_tests: PASS");
    return 0;
}

