#include "hal_camera_jpeg_integrity.h"

#define JPEG_MARKER_BYTES 2U
#define JPEG_MINIMUM_BYTES 4U
#define JPEG_OFFSET_NOT_FOUND ((size_t) - 1)

static bool marker_has_dimensions(uint8_t marker) {
    return (marker >= 0xC0U && marker <= 0xC3U) || (marker >= 0xC5U && marker <= 0xC7U) ||
           (marker >= 0xC9U && marker <= 0xCBU) || (marker >= 0xCDU && marker <= 0xCFU);
}

static void result_reset(hal_camera_jpeg_integrity_result_t *result, size_t size) {
    result->status = HAL_CAMERA_JPEG_INTEGRITY_TOO_SHORT;
    result->input_size = size;
    result->normalized_size = 0U;
    result->eoi_offset = JPEG_OFFSET_NOT_FOUND;
    result->trailing_bytes = 0U;
}

bool hal_camera_jpeg_normalize(const uint8_t *jpeg, size_t size, hal_camera_jpeg_integrity_result_t *result) {
    size_t offset;

    if (result == NULL) {
        return false;
    }
    result_reset(result, size);
    if (jpeg == NULL || size < JPEG_MINIMUM_BYTES) {
        return false;
    }
    if (jpeg[0] != 0xFFU || jpeg[1] != 0xD8U) {
        result->status = HAL_CAMERA_JPEG_INTEGRITY_BAD_SOI;
        return false;
    }

    offset = size - JPEG_MARKER_BYTES;
    for (;;) {
        if (jpeg[offset] == 0xFFU && jpeg[offset + 1U] == 0xD9U) {
            result->eoi_offset = offset;
            result->normalized_size = offset + JPEG_MARKER_BYTES;
            result->trailing_bytes = size - result->normalized_size;
            result->status = result->trailing_bytes == 0U ? HAL_CAMERA_JPEG_INTEGRITY_COMPLETE
                                                          : HAL_CAMERA_JPEG_INTEGRITY_TRAILING_BYTES;
            return true;
        }
        if (offset <= JPEG_MARKER_BYTES) {
            break;
        }
        --offset;
    }

    result->status = HAL_CAMERA_JPEG_INTEGRITY_MISSING_EOI;
    return false;
}

bool hal_camera_jpeg_read_dimensions(const uint8_t *jpeg, size_t size, uint16_t *width, uint16_t *height) {
    size_t offset = JPEG_MARKER_BYTES;

    if (jpeg == NULL || width == NULL || height == NULL || size < JPEG_MINIMUM_BYTES || jpeg[0] != 0xFFU ||
        jpeg[1] != 0xD8U) {
        return false;
    }
    while (offset + 1U < size) {
        uint8_t marker;
        uint16_t segment_size;

        while (offset < size && jpeg[offset] == 0xFFU) {
            ++offset;
        }
        if (offset >= size) {
            return false;
        }
        marker = jpeg[offset++];
        if (marker == 0x00U) {
            continue;
        }
        if (marker == 0xD9U || marker == 0xDAU) {
            return false;
        }
        if (marker == 0x01U || (marker >= 0xD0U && marker <= 0xD7U)) {
            continue;
        }
        if (offset + 2U > size) {
            return false;
        }
        segment_size = (uint16_t)(((uint16_t)jpeg[offset] << 8U) | jpeg[offset + 1U]);
        if (segment_size < 2U || offset + segment_size > size) {
            return false;
        }
        if (marker_has_dimensions(marker)) {
            if (segment_size < 7U) {
                return false;
            }
            *height = (uint16_t)(((uint16_t)jpeg[offset + 3U] << 8U) | jpeg[offset + 4U]);
            *width = (uint16_t)(((uint16_t)jpeg[offset + 5U] << 8U) | jpeg[offset + 6U]);
            return *width != 0U && *height != 0U;
        }
        offset += segment_size;
    }
    return false;
}

bool hal_camera_jpeg_dimensions_match(const uint8_t *jpeg, size_t size, uint16_t expected_width,
                                      uint16_t expected_height) {
    uint16_t width = 0U;
    uint16_t height = 0U;

    return hal_camera_jpeg_read_dimensions(jpeg, size, &width, &height) && width == expected_width &&
           height == expected_height;
}

const char *hal_camera_jpeg_integrity_status_name(hal_camera_jpeg_integrity_status_t status) {
    switch (status) {
    case HAL_CAMERA_JPEG_INTEGRITY_COMPLETE:
        return "complete";
    case HAL_CAMERA_JPEG_INTEGRITY_TRAILING_BYTES:
        return "trailing_bytes";
    case HAL_CAMERA_JPEG_INTEGRITY_TOO_SHORT:
        return "too_short";
    case HAL_CAMERA_JPEG_INTEGRITY_BAD_SOI:
        return "bad_soi";
    case HAL_CAMERA_JPEG_INTEGRITY_MISSING_EOI:
        return "missing_eoi";
    default:
        return "unknown";
    }
}

