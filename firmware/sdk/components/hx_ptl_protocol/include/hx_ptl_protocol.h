#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hx_media_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HX_PTL_BASE_HEADER_SIZE 7U
#define HX_PTL_MAGIC_0 0xc0U
#define HX_PTL_MAGIC_1 0x5aU
#define HX_PTL_DATA_TYPE_JPEG 0x01U

typedef enum {
    HX_PTL_OK = 0,
    HX_PTL_ERR_ARGUMENT,
    HX_PTL_ERR_BASE_HEADER_SIZE,
    HX_PTL_ERR_EMPTY,
    HX_PTL_ERR_MAGIC,
    HX_PTL_ERR_MEDIA_TYPE,
    HX_PTL_ERR_BODY_SIZE,
    HX_PTL_ERR_MEDIA_HEADER,
    HX_PTL_ERR_JPEG,
} hx_ptl_result_t;

typedef struct {
    uint8_t media_type;
    uint32_t body_size;
    uint32_t payload_size;
} hx_ptl_base_header_t;

typedef struct {
    hx_ptl_base_header_t ptl;
    hx_media_header_t media;
    const uint8_t *payload;
    size_t payload_size;
} hx_ptl_frame_t;

hx_ptl_result_t hx_ptl_parse_base_header(const uint8_t *header, size_t header_size, size_t max_payload_size,
                                         hx_ptl_base_header_t *decoded);

hx_ptl_result_t hx_ptl_validate_jpeg_packet(const uint8_t *base_header, size_t base_header_size, const uint8_t *body,
                                            size_t body_size, size_t max_payload_size, hx_ptl_frame_t *frame);

const char *hx_ptl_result_name(hx_ptl_result_t result);

#ifdef __cplusplus
}
#endif

