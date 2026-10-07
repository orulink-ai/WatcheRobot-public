idf_component_register(
    SRCS "src/mcu_motion_service.c"
    INCLUDE_DIRS "include"
    REQUIRES mcu_link motion_arbiter
)

