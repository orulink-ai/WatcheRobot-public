idf_component_register(
    SRCS
        "src/mcu_crc16.c"
        "src/mcu_cobs.c"
        "src/mcu_frame.c"
        "src/mcu_wire.c"
        "src/mcu_link_fsm.c"
        "src/mcu_link_stats.c"
        "src/mcu_link.c"
        "src/mcu_link_uart.c"
        "src/mcu_link_bootstrap.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_driver_uart
)

