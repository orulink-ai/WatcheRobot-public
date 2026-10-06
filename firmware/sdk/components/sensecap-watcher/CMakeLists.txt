set(requires "esp_mm" "esp_psram" "esp_lcd" "fatfs" "esp_timer" "esp_driver_gpio" "esp_driver_sdspi" "esp_driver_ledc" "esp_driver_uart" "esp_driver_i2c" "esp_driver_i2s" "esp_driver_spi" "spiffs" "lvgl" "esp_io_expander_pca95xx_16bit" "esp_lvgl_port" "sscma_client")

# IDF6 触摸回归修复：配置阶段幂等 patch managed_component 的 SPD2010 i2c_read 宏。
list(APPEND requires esp_driver_rmt esp_hal_gpspi heap)
# 详见 patches.cmake 与 scripts/patch_spd2010_idf6_touch.py。
include("${CMAKE_CURRENT_LIST_DIR}/patches.cmake")

idf_component_register(
    SRCS
        "sensecap-watcher.c"
        "io_expander_read_guard.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "${IDF_PATH}/components/fatfs/vfs"
    REQUIRES ${requires}
)

