#include "watcher_hw_display.h"
#include "esp_lvgl_port.h"
#include "sensecap-watcher.h"
static watcher_hw_status_t state;
static bool owned;
esp_err_t watcher_hw_display_init(void) {
    if (state.state == WATCHER_HW_READY)
        return ESP_OK;
    if (owned)
        return ESP_ERR_INVALID_STATE;
    if (bsp_lvgl_get_disp() != NULL)
        return ESP_ERR_INVALID_STATE;
    owned = true;
    esp_err_t ret = bsp_lvgl_init() != NULL ? ESP_OK : ESP_FAIL;
    if (ret == ESP_OK && (bsp_lcd_get_touch_handle() == NULL || bsp_lvgl_get_touch_indev() == NULL))
        ret = ESP_ERR_NOT_FOUND;
    if (ret == ESP_OK)
        ret = bsp_lcd_brightness_set(60);
    if (ret != ESP_OK && bsp_lvgl_deinit() == ESP_OK)
        owned = false;
    state = (watcher_hw_status_t){ret == ESP_OK ? WATCHER_HW_READY : WATCHER_HW_FAULT, ret};
    return ret;
}
esp_err_t watcher_hw_display_close(void) {
    if (!owned) {
        state = (watcher_hw_status_t){WATCHER_HW_CLOSED, ESP_OK};
        return ESP_OK;
    }
    esp_err_t ret = bsp_lvgl_deinit();
    if (ret == ESP_OK)
        owned = false;
    state = (watcher_hw_status_t){ret == ESP_OK ? WATCHER_HW_CLOSED : WATCHER_HW_FAULT, ret};
    return ret;
}
esp_err_t watcher_hw_display_status(watcher_hw_status_t *out) {
    if (out == NULL)
        return ESP_ERR_INVALID_ARG;
    *out = state;
    return ESP_OK;
}
lv_disp_t *watcher_hw_display_lvgl(void) {
    return state.state == WATCHER_HW_READY ? bsp_lvgl_get_disp() : NULL;
}
esp_lcd_panel_handle_t watcher_hw_display_panel(void) {
    return state.state == WATCHER_HW_READY ? bsp_lcd_get_panel_handle() : NULL;
}
esp_lcd_touch_handle_t watcher_hw_display_touch(void) {
    return state.state == WATCHER_HW_READY ? bsp_lcd_get_touch_handle() : NULL;
}
bool watcher_hw_display_lock(uint32_t timeout_ms) {
    return state.state == WATCHER_HW_READY && lvgl_port_lock(timeout_ms);
}
void watcher_hw_display_unlock(void) {
    lvgl_port_unlock();
}
