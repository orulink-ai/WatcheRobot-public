#pragma once
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "watcher_hw_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Single application owner for lifecycle. Borrowed handles valid until close.
 * SDK owns LVGL flushing: do not independently draw to the panel concurrently.
 * Protect LVGL operations with lock/unlock. Never close while holding lock,
 * from LVGL callback, or while another task uses a borrowed handle. */
esp_err_t watcher_hw_display_init(void);
esp_err_t watcher_hw_display_close(void);
esp_err_t watcher_hw_display_status(watcher_hw_status_t *status);
lv_disp_t *watcher_hw_display_lvgl(void);
esp_lcd_panel_handle_t watcher_hw_display_panel(void);
esp_lcd_touch_handle_t watcher_hw_display_touch(void);
bool watcher_hw_display_lock(uint32_t timeout_ms);
void watcher_hw_display_unlock(void);
#ifdef __cplusplus
}
#endif
