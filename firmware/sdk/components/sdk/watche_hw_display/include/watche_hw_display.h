#pragma once
#include "watche_hw_core.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Single application owner for lifecycle. Borrowed handles valid until close.
 * SDK owns LVGL flushing: do not independently draw to the panel concurrently.
 * Protect LVGL operations with lock/unlock. Never close while holding lock,
 * from LVGL callback, or while another task uses a borrowed handle. */
esp_err_t watche_hw_display_init(void);
esp_err_t watche_hw_display_close(void);
esp_err_t watche_hw_display_status(watche_hw_status_t *status);
lv_disp_t *watche_hw_display_lvgl(void);
esp_lcd_panel_handle_t watche_hw_display_panel(void);
esp_lcd_touch_handle_t watche_hw_display_touch(void);
bool watche_hw_display_lock(uint32_t timeout_ms);
void watche_hw_display_unlock(void);
#ifdef __cplusplus
}
#endif

