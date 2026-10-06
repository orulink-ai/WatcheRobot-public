#include "sensecap-watcher.h"
#include <assert.h>
static lv_disp_t display;
static bool active, fail_init, missing_touch, missing_indev;
static esp_err_t brightness_error, close_error;
static unsigned inits, closes;
lv_disp_t *bsp_lvgl_get_disp(void) { return active ? &display : NULL; }
lv_disp_t *bsp_lvgl_init(void) { inits++; active = !fail_init; return bsp_lvgl_get_disp(); }
esp_err_t bsp_lvgl_deinit(void) { closes++; if (!close_error) active=false; return close_error; }
esp_err_t bsp_lcd_brightness_set(int value) { assert(value==60); return brightness_error; }
esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void) { return active ? &display : NULL; }
esp_lcd_touch_handle_t bsp_lcd_get_touch_handle(void) { return active && !missing_touch ? &display : NULL; }
lv_indev_t *bsp_lvgl_get_touch_indev(void) { return active && !missing_indev ? (lv_indev_t *)&display : NULL; }
bool lvgl_port_lock(uint32_t timeout) { (void)timeout; return true; }
void lvgl_port_unlock(void) {}
int main(void) {
    watche_hw_status_t state;
    assert(watche_hw_display_status(NULL)==ESP_ERR_INVALID_ARG);
    assert(!watche_hw_display_lock(10));
    active=true;
    assert(watche_hw_display_init()==ESP_ERR_INVALID_STATE && inits==0);
    assert(watche_hw_display_close()==ESP_OK && active); /* Foreign owner remains intact. */
    active=false; fail_init=true;
    assert(watche_hw_display_init()==ESP_FAIL && closes==1);
    fail_init=false; missing_touch=true;
    assert(watche_hw_display_init()==ESP_ERR_NOT_FOUND && !active);
    missing_touch=false; missing_indev=true;
    assert(watche_hw_display_init()==ESP_ERR_NOT_FOUND && !active);
    missing_indev=false; brightness_error=ESP_FAIL;
    assert(watche_hw_display_init()==ESP_FAIL && !active);
    brightness_error=ESP_OK;
    fail_init=true; close_error=ESP_FAIL;
    assert(watche_hw_display_init()==ESP_FAIL);
    fail_init=false;
    assert(watche_hw_display_init()==ESP_ERR_INVALID_STATE);
    close_error=ESP_OK; assert(watche_hw_display_close()==ESP_OK);
    for (int i=0;i<3;i++) {
        assert(watche_hw_display_init()==ESP_OK);
        unsigned before=inits;
        assert(watche_hw_display_init()==ESP_OK && inits==before);
        assert(watche_hw_display_lvgl()==&display && watche_hw_display_touch());
        assert(watche_hw_display_lock(10)); watche_hw_display_unlock();
        close_error=ESP_FAIL;
        assert(watche_hw_display_close()==ESP_FAIL && active);
        assert(watche_hw_display_lvgl()==NULL);
        assert(watche_hw_display_status(&state)==ESP_OK && state.state==WATCHE_HW_FAULT);
        close_error=ESP_OK;
        assert(watche_hw_display_close()==ESP_OK && !active);
        assert(watche_hw_display_close()==ESP_OK);
    }
    return 0;
}

