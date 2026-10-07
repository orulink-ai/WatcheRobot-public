#include "hal_camera.h"
#include <assert.h>
static esp_err_t init_result, close_result;
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
esp_err_t hal_camera_capture_once(watche_hw_camera_frame_cb_t cb, void *ctx) {
    const uint8_t jpeg[] = {255, 216, 255, 217};
    captures++;
    cb(jpeg, 4, 1, ctx);
    return ESP_OK;
}
esp_err_t hal_camera_start(int fps, watche_hw_camera_frame_cb_t cb, void *ctx) {
    (void)fps;
    (void)cb;
    (void)ctx;
    starts++;
    return ESP_OK;
}
esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *out) {
    out->connected = connected;
    out->initialized = initialized;
    return ESP_OK;
}
static void frame(const uint8_t *data, size_t size, uint32_t ts, void *ctx) {
    (void)ctx;
    assert(size == 4 && ts == 1 && data[0] == 255);
}
int main(void) {
    watche_hw_status_t state;
    assert(watche_hw_camera_status(NULL) == ESP_ERR_INVALID_ARG);
    assert(watche_hw_camera_capture(frame, NULL) == ESP_ERR_INVALID_STATE);
    initialized = true;
    assert(watche_hw_camera_init() == ESP_ERR_INVALID_STATE);
    assert(watche_hw_camera_stop() == ESP_ERR_INVALID_STATE && stops == 0);
    assert(watche_hw_camera_close() == ESP_OK && initialized && cleans == 0);
    initialized = false;
    init_result = ESP_FAIL;
    assert(watche_hw_camera_init() == ESP_FAIL && cleans == 1);
    assert(watche_hw_camera_status(&state) == ESP_OK && state.state == WATCHE_HW_FAULT);
    init_result = ESP_OK;
    for (int i = 0; i < 3; i++) {
        assert(watche_hw_camera_init() == ESP_OK);
        assert(watche_hw_camera_capture(NULL, NULL) == ESP_ERR_INVALID_ARG);
        assert(watche_hw_camera_stream(0, frame, NULL) == ESP_ERR_INVALID_ARG);
        assert(watche_hw_camera_stream(31, frame, NULL) == ESP_ERR_INVALID_ARG);
        assert(watche_hw_camera_capture(frame, NULL) == ESP_OK);
        assert(watche_hw_camera_stream(5, frame, NULL) == ESP_OK);
        connected = false;
        assert(watche_hw_camera_status(&state) == ESP_OK && state.state == WATCHE_HW_FAULT);
        connected = true;
        close_result = ESP_FAIL;
        assert(watche_hw_camera_close() == ESP_FAIL);
        assert(watche_hw_camera_init() == ESP_ERR_INVALID_STATE);
        close_result = ESP_OK;
        assert(watche_hw_camera_close() == ESP_OK);
        assert(watche_hw_camera_capture(frame, NULL) == ESP_ERR_INVALID_STATE);
    }
    assert(captures == 3 && starts == 3);
    return 0;
}
