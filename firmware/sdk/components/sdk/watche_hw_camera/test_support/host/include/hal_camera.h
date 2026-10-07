#pragma once
#include "watche_hw_camera.h"
typedef struct {
    bool connected;
    bool initialized;
} hal_camera_diagnostics_t;
esp_err_t hal_camera_init(void);
esp_err_t hal_camera_deinit(void);
esp_err_t hal_camera_capture_once(watche_hw_camera_frame_cb_t, void *);
esp_err_t hal_camera_start(int, watche_hw_camera_frame_cb_t, void *);
esp_err_t hal_camera_stop(void);
esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *);
