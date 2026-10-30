#include "watcher_hw_camera.h"
#include "hal_camera.h"
static watcher_hw_status_t state;
static bool owned;
esp_err_t watcher_hw_camera_init(void) {
    if (owned)
        return state.state == WATCHER_HW_READY ? ESP_OK : ESP_ERR_INVALID_STATE;
    hal_camera_diagnostics_t diagnostics;
    esp_err_t ret = hal_camera_get_diagnostics(&diagnostics);
    if (ret != ESP_OK)
        return ret;
    if (diagnostics.initialized)
        return ESP_ERR_INVALID_STATE;
    ret = hal_camera_init();
    owned = true;
    if (ret != ESP_OK && hal_camera_deinit() == ESP_OK)
        owned = false;
    state = (watcher_hw_status_t){ret == ESP_OK ? WATCHER_HW_READY : WATCHER_HW_FAULT, ret};
    return ret;
}
esp_err_t watcher_hw_camera_close(void) {
    if (!owned) {
        state = (watcher_hw_status_t){WATCHER_HW_CLOSED, ESP_OK};
        return ESP_OK;
    }
    esp_err_t ret = hal_camera_deinit();
    if (ret == ESP_OK)
        owned = false;
    state = (watcher_hw_status_t){ret == ESP_OK ? WATCHER_HW_CLOSED : WATCHER_HW_FAULT, ret};
    return ret;
}
esp_err_t watcher_hw_camera_capture(watcher_hw_camera_frame_cb_t cb, void *ctx) {
    if (cb == NULL)
        return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHER_HW_READY)
        return ESP_ERR_INVALID_STATE;
    state.last_error = hal_camera_capture_once(cb, ctx);
    return state.last_error;
}
esp_err_t watcher_hw_camera_stream(int fps, watcher_hw_camera_frame_cb_t cb, void *ctx) {
    if (cb == NULL || fps < 1 || fps > 30)
        return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHER_HW_READY)
        return ESP_ERR_INVALID_STATE;
    state.last_error = hal_camera_start(fps, cb, ctx);
    return state.last_error;
}
esp_err_t watcher_hw_camera_stop(void) {
    if (!owned)
        return ESP_ERR_INVALID_STATE;
    state.last_error = hal_camera_stop();
    return state.last_error;
}
esp_err_t watcher_hw_camera_status(watcher_hw_status_t *out) {
    if (out == NULL)
        return ESP_ERR_INVALID_ARG;
    *out = state;
    if (state.state == WATCHER_HW_READY) {
        hal_camera_diagnostics_t diagnostics;
        esp_err_t ret = hal_camera_get_diagnostics(&diagnostics);
        if (ret == ESP_OK)
            ret = !diagnostics.connected ? ESP_ERR_INVALID_STATE : diagnostics.status;
        if (ret != ESP_OK) {
            out->state = WATCHER_HW_FAULT;
            out->last_error = ret;
        }
    }
    return ESP_OK;
}
