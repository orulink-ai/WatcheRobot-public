idf_component_register(SRCS "watche_hw_body.c" INCLUDE_DIRS "include"
    REQUIRES watche_hw_core mcu_runtime mcu_motion_service mcu_led_service mcu_sensor_service
    PRIV_REQUIRES freertos esp_timer motion_arbiter)

