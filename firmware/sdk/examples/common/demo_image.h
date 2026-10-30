#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DEMO_JPEG_LIMIT (256U * 1024U)
typedef struct { uint8_t *pixels; unsigned width, height; size_t bytes; } demo_image_t;
/* One media owner. Output owns PSRAM pixels on success, empty on failure.
 * No LVGL calls; caller installs/replaces pixels under the display lock. */
bool demo_jpeg_valid(const uint8_t *jpeg, size_t bytes);
esp_err_t demo_image_decode(const uint8_t *jpeg, size_t bytes, bool swap, demo_image_t *output);
esp_err_t demo_image_capture(bool swap, demo_image_t *output);
void demo_image_release(demo_image_t *image);
