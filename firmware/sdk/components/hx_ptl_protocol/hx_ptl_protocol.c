#include "hx_ptl_protocol.h"

#include <stdbool.h>

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

hx_ptl_result_t hx_ptl_parse_base_header(const uint8_t *header, size_t header_size, size_t max_payload_size,
                                         hx_ptl_base_header_t *decoded) {
    hx_ptl_base_header_t value;

    if (header == NULL || decoded == NULL) {
        return HX_PTL_ERR_ARGUMENT;
    }
    if (header_size != HX_PTL_BASE_HEADER_SIZE) {
        return HX_PTL_ERR_BASE_HEADER_SIZE;
    }
    bool empty = true;
    for (size_t i = 0; i < header_size; ++i) {
        if (header[i] != 0U) {
            empty = false;
            break;
        }
    }
    if (empty) {
        return HX_PTL_ERR_EMPTY;
    }
    if (header[0] != HX_PTL_MAGIC_0 || header[1] != HX_PTL_MAGIC_1) {
        return HX_PTL_ERR_MAGIC;
    }
    if (header[2] != HX_PTL_DATA_TYPE_JPEG) {
        return HX_PTL_ERR_MEDIA_TYPE;
    }

    value.media_type = header[2];
    /* write_simple_ex() reports the complete body size: optional user header
     * plus the raw (and potentially alignment-padded) media data. */
    value.body_size = read_le32(&header[3]);
    if (value.body_size < HX_MEDIA_HEADER_SIZE || (size_t)(value.body_size - HX_MEDIA_HEADER_SIZE) > max_payload_size) {
        return HX_PTL_ERR_BODY_SIZE;
    }
    value.payload_size = value.body_size - HX_MEDIA_HEADER_SIZE;
    *decoded = value;
    return HX_PTL_OK;
}

hx_ptl_result_t hx_ptl_validate_jpeg_packet(const uint8_t *base_header, size_t base_header_size, const uint8_t *body,
                                            size_t body_size, size_t max_payload_size, hx_ptl_frame_t *frame) {
    hx_ptl_base_header_t ptl;
    hx_media_result_t media_result;
    hx_ptl_result_t result;
    size_t jpeg_size;

    if (body == NULL || frame == NULL) {
        return HX_PTL_ERR_ARGUMENT;
    }
    result = hx_ptl_parse_base_header(base_header, base_header_size, max_payload_size, &ptl);
    if (result != HX_PTL_OK) {
        return result;
    }
    if (body_size != (size_t)ptl.body_size) {
        return HX_PTL_ERR_BODY_SIZE;
    }

    frame->ptl = ptl;
    frame->payload = &body[HX_MEDIA_HEADER_SIZE];
    jpeg_size = ptl.payload_size;
    while (jpeg_size >= 2U && !(frame->payload[jpeg_size - 2U] == 0xffU && frame->payload[jpeg_size - 1U] == 0xd9U)) {
        --jpeg_size;
    }
    if (jpeg_size < 4U || frame->payload[0] != 0xffU || frame->payload[1] != 0xd8U ||
        frame->payload[jpeg_size - 2U] != 0xffU || frame->payload[jpeg_size - 1U] != 0xd9U) {
        return HX_PTL_ERR_JPEG;
    }
    /* The authenticated HX payload may contain alignment bytes after EOI.
     * Validate the full body, but expose only the JPEG boundary downstream. */
    media_result = hx_media_frame_validate(body, HX_MEDIA_HEADER_SIZE, frame->payload, ptl.payload_size,
                                           max_payload_size, &frame->media);
    if (media_result != HX_MEDIA_OK) {
        return HX_PTL_ERR_MEDIA_HEADER;
    }
    frame->payload_size = jpeg_size;
    return HX_PTL_OK;
}

const char *hx_ptl_result_name(hx_ptl_result_t result) {
    switch (result) {
    case HX_PTL_OK:
        return "ok";
    case HX_PTL_ERR_ARGUMENT:
        return "argument";
    case HX_PTL_ERR_BASE_HEADER_SIZE:
        return "base_header_size";
    case HX_PTL_ERR_EMPTY:
        return "empty";
    case HX_PTL_ERR_MAGIC:
        return "magic";
    case HX_PTL_ERR_MEDIA_TYPE:
        return "media_type";
    case HX_PTL_ERR_BODY_SIZE:
        return "body_size";
    case HX_PTL_ERR_MEDIA_HEADER:
        return "media_header";
    case HX_PTL_ERR_JPEG:
        return "jpeg";
    default:
        return "unknown";
    }
}

