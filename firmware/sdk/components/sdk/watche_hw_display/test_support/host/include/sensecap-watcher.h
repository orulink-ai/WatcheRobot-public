#pragma once
#include "watche_hw_display.h"
lv_disp_t *bsp_lvgl_get_disp(void);
lv_indev_t *bsp_lvgl_get_touch_indev(void);
lv_disp_t *bsp_lvgl_init(void);
esp_err_t bsp_lvgl_deinit(void);
esp_err_t bsp_lcd_brightness_set(int value);
esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void);
esp_lcd_touch_handle_t bsp_lcd_get_touch_handle(void);

