#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
esp_err_t watcher_hx_control_mode_preview(unsigned mode, unsigned model, bool preview);
/* Called by the single camera lifecycle owner; UART1 is exclusive with the
 * maintenance flasher. Stop the frame reader before changing modes. */
esp_err_t watcher_hx_control_init(void);
esp_err_t watcher_hx_control_mode(unsigned mode, unsigned model);
void watcher_hx_control_deinit(void);
typedef struct {
    uint16_t x, y, width, height;
    uint8_t score, target;
} watcher_hx_box_t;
typedef struct {
    uint32_t generation, sequence;
    uint16_t width, height;
    uint8_t count;
    watcher_hx_box_t boxes[8];
} watcher_hx_result_t;
esp_err_t watcher_hx_control_boxes(watcher_hx_result_t *out);
esp_err_t watcher_hx_control_catalog(uint32_t crc[4]);
uint32_t watcher_hx_control_generation(void);
typedef struct {
    uint32_t generation;
    unsigned mode, model, error;
} watcher_hx_state_t;
esp_err_t watcher_hx_control_status(watcher_hx_state_t *out);

