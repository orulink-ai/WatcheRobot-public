#pragma once
#include "watche_hw_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* JPEG bytes are borrowed ONLY during callback; copy before returning.
 * Callback runs on camera worker (stream) or calling task (capture).
 * Do not stop/close/start recursively from callback. Serialize lifecycle calls
 * in one application owner task. No UI, AI/model or network dependency. */
typedef void (*watche_hw_camera_frame_cb_t)(const uint8_t *jpeg, size_t size, uint32_t timestamp_ms, void *ctx);
esp_err_t watche_hw_camera_init(void);
esp_err_t watche_hw_camera_close(void);
esp_err_t watche_hw_camera_capture(watche_hw_camera_frame_cb_t callback, void *ctx);
esp_err_t watche_hw_camera_stream(int fps, watche_hw_camera_frame_cb_t callback, void *ctx);
esp_err_t watche_hw_camera_stop(void);
esp_err_t watche_hw_camera_status(watche_hw_status_t *status);
#ifdef __cplusplus
}
#endif
