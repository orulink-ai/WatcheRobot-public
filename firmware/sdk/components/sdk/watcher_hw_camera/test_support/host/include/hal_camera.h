#pragma once
#include "watcher_hw_camera.h"
typedef struct {
    bool connected;
    bool initialized;
    bool streaming;
    esp_err_t status;
} hal_camera_diagnostics_t;
esp_err_t hal_camera_init(void);
esp_err_t hal_camera_deinit(void);
esp_err_t hal_camera_capture_once(watcher_hw_camera_frame_cb_t, void *);
esp_err_t hal_camera_start(int, watcher_hw_camera_frame_cb_t, void *);
esp_err_t hal_camera_stop(void);
esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *);
