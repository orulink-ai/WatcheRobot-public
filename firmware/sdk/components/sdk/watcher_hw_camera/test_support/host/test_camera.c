#include "hal_camera.h"
#include <assert.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
static esp_err_t init_result, close_result, diagnostic_result, device_status;
static bool connected = true;
static bool initialized;
static int cleans, captures, starts, stops;
esp_err_t hal_camera_init(void) {
    if (!init_result)
        initialized = true;
    return init_result;
}
esp_err_t hal_camera_deinit(void) {
    cleans++;
    if (!close_result)
        initialized = false;
    return close_result;
}
esp_err_t hal_camera_stop(void) {
    stops++;
    return close_result;
}
esp_err_t hal_camera_capture_once(watcher_hw_camera_frame_cb_t cb, void *ctx) {
    const uint8_t jpeg[] = {255, 216, 255, 217};
    captures++;
    cb(jpeg, 4, 1, ctx);
    return ESP_OK;
}
esp_err_t hal_camera_start(int fps, watcher_hw_camera_frame_cb_t cb, void *ctx) {
    (void)fps;
    (void)cb;
    (void)ctx;
    starts++;
    return ESP_OK;
}
esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *out) {
    if (diagnostic_result != ESP_OK)
        return diagnostic_result; /* Do not populate output on a query failure. */
    out->connected = connected;
    out->initialized = initialized;
    out->streaming = false; /* Idle is a valid ready camera. */
    out->status = device_status;
    return ESP_OK;
}
static void frame(const uint8_t *data, size_t size, uint32_t ts, void *ctx) {
    (void)ctx;
    assert(size == 4 && ts == 1 && data[0] == 255);
}
int main(void) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    watcher_hw_status_t state;
    assert(watcher_hw_camera_status(NULL) == ESP_ERR_INVALID_ARG);
    assert(watcher_hw_camera_capture(frame, NULL) == ESP_ERR_INVALID_STATE);
    initialized = true;
    assert(watcher_hw_camera_init() == ESP_ERR_INVALID_STATE);
    assert(watcher_hw_camera_stop() == ESP_ERR_INVALID_STATE && stops == 0);
    assert(watcher_hw_camera_close() == ESP_OK && initialized && cleans == 0);
    initialized = false;
    init_result = ESP_FAIL;
    assert(watcher_hw_camera_init() == ESP_FAIL && cleans == 1);
    assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_FAULT);
    init_result = ESP_OK;
    for (int i = 0; i < 3; i++) {
        assert(watcher_hw_camera_init() == ESP_OK);
        assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_READY);
        assert(watcher_hw_camera_capture(NULL, NULL) == ESP_ERR_INVALID_ARG);
        assert(watcher_hw_camera_stream(0, frame, NULL) == ESP_ERR_INVALID_ARG);
        assert(watcher_hw_camera_stream(31, frame, NULL) == ESP_ERR_INVALID_ARG);
        assert(watcher_hw_camera_capture(frame, NULL) == ESP_OK);
        assert(watcher_hw_camera_stream(5, frame, NULL) == ESP_OK);
        connected = false;
        device_status = ESP_FAIL;
        assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_FAULT &&
               state.last_error == ESP_ERR_INVALID_STATE); /* Connection takes precedence. */
        connected = true;
        assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_FAULT &&
               state.last_error == ESP_FAIL); /* Connected does not mean healthy. */
        diagnostic_result = ESP_ERR_TIMEOUT;
        assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_FAULT &&
               state.last_error == ESP_ERR_TIMEOUT); /* Query failure takes precedence. */
        diagnostic_result = ESP_OK;
        device_status = ESP_OK;
        assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_READY &&
               state.last_error == ESP_OK); /* Transient diagnostic fault can recover. */
        close_result = ESP_FAIL;
        assert(watcher_hw_camera_close() == ESP_FAIL);
        assert(watcher_hw_camera_status(&state) == ESP_OK && state.state == WATCHER_HW_FAULT);
        assert(watcher_hw_camera_init() == ESP_ERR_INVALID_STATE);
        close_result = ESP_OK;
        assert(watcher_hw_camera_close() == ESP_OK);
        assert(watcher_hw_camera_capture(frame, NULL) == ESP_ERR_INVALID_STATE);
    }
    assert(captures == 3 && starts == 3);
    return 0;
}
