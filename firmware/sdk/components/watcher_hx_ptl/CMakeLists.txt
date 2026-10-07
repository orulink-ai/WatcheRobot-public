idf_component_register(
    SRCS "watcher_hx_ptl.c" "watcher_hx_control.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_uart esp_io_expander esp_timer freertos
             heap hx_media_protocol hx_ptl_protocol log sensecap-watcher
)


