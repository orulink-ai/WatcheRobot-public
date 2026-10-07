idf_component_register(
    SRCS "src/mcu_led_service.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos mcu_link
)

