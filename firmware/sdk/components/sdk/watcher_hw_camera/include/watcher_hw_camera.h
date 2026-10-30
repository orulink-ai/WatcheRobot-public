#pragma once
#include "watcher_hw_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* JPEG bytes are borrowed ONLY during callback; copy before returning.
 * Callback runs on camera worker (stream) or calling task (capture).
 * Do not stop/close/start recursively from callback. Serialize lifecycle calls
 * in one application owner task. No UI, AI/model or network dependency. */
typedef void (*watcher_hw_camera_frame_cb_t)(const uint8_t *jpeg, size_t size, uint32_t timestamp_ms, void *ctx);
esp_err_t watcher_hw_camera_init(void);
esp_err_t watcher_hw_camera_close(void);
esp_err_t watcher_hw_camera_capture(watcher_hw_camera_frame_cb_t callback, void *ctx);
esp_err_t watcher_hw_camera_stream(int fps, watcher_hw_camera_frame_cb_t callback, void *ctx);
esp_err_t watcher_hw_camera_stop(void);
/* Reports current HAL query/connection/device faults even if locally open.
 * Idle is READY; streaming is not required. Transient diagnostic faults clear
 * on a healthy query; lifecycle failures still require close/re-init. Return
 * ESP_OK means the status was obtained, not that the camera is READY. */
esp_err_t watcher_hw_camera_status(watcher_hw_status_t *status);
#ifdef __cplusplus
}
#endif
