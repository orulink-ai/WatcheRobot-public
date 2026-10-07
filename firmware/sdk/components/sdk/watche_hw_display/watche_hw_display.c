#include "watche_hw_display.h"
#include "esp_lvgl_port.h"
#include "sensecap-watcher.h"
static watche_hw_status_t state;
static bool owned;
esp_err_t watche_hw_display_init(void) {
    if (state.state == WATCHE_HW_READY)
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
    state = (watche_hw_status_t){ret == ESP_OK ? WATCHE_HW_READY : WATCHE_HW_FAULT, ret};
    return ret;
}
esp_err_t watche_hw_display_close(void) {
    if (!owned) {
        state = (watche_hw_status_t){WATCHE_HW_CLOSED, ESP_OK};
        return ESP_OK;
    }
    esp_err_t ret = bsp_lvgl_deinit();
    if (ret == ESP_OK)
        owned = false;
    state = (watche_hw_status_t){ret == ESP_OK ? WATCHE_HW_CLOSED : WATCHE_HW_FAULT, ret};
    return ret;
}
esp_err_t watche_hw_display_status(watche_hw_status_t *out) {
    if (out == NULL)
        return ESP_ERR_INVALID_ARG;
    *out = state;
    return ESP_OK;
}
lv_disp_t *watche_hw_display_lvgl(void) {
    return state.state == WATCHE_HW_READY ? bsp_lvgl_get_disp() : NULL;
}
esp_lcd_panel_handle_t watche_hw_display_panel(void) {
    return state.state == WATCHE_HW_READY ? bsp_lcd_get_panel_handle() : NULL;
}
esp_lcd_touch_handle_t watche_hw_display_touch(void) {
    return state.state == WATCHE_HW_READY ? bsp_lcd_get_touch_handle() : NULL;
}
bool watche_hw_display_lock(uint32_t timeout_ms) {
    return state.state == WATCHE_HW_READY && lvgl_port_lock(timeout_ms);
}
void watche_hw_display_unlock(void) {
    lvgl_port_unlock();
}
