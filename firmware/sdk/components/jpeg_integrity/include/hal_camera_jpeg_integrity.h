#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HAL_CAMERA_JPEG_INTEGRITY_COMPLETE = 0,
    HAL_CAMERA_JPEG_INTEGRITY_TRAILING_BYTES,
    HAL_CAMERA_JPEG_INTEGRITY_TOO_SHORT,
    HAL_CAMERA_JPEG_INTEGRITY_BAD_SOI,
    HAL_CAMERA_JPEG_INTEGRITY_MISSING_EOI,
} hal_camera_jpeg_integrity_status_t;

typedef struct {
    hal_camera_jpeg_integrity_status_t status;
    size_t input_size;
    size_t normalized_size;
    size_t eoi_offset;
    size_t trailing_bytes;
} hal_camera_jpeg_integrity_result_t;

bool hal_camera_jpeg_normalize(const uint8_t *jpeg, size_t size, hal_camera_jpeg_integrity_result_t *result);
bool hal_camera_jpeg_read_dimensions(const uint8_t *jpeg, size_t size, uint16_t *width, uint16_t *height);
bool hal_camera_jpeg_dimensions_match(const uint8_t *jpeg, size_t size, uint16_t expected_width,
                                      uint16_t expected_height);
const char *hal_camera_jpeg_integrity_status_name(hal_camera_jpeg_integrity_status_t status);

