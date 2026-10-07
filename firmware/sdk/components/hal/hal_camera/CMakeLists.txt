if(CONFIG_WATCHER_CAMERA_BACKEND_PTL)
    set(WATCHER_CAMERA_SOURCE "src/hal_camera_ptl.c")
else()
    set(WATCHER_CAMERA_SOURCE "src/hal_camera.c")
endif()

idf_component_register(
    SRCS "${WATCHER_CAMERA_SOURCE}"
    INCLUDE_DIRS "include"
    REQUIRES
        sensecap-watcher
        sscma_client
        mbedtls
        esp_timer
        watcher_hx_ptl
        hx_media_protocol
        hx_ptl_protocol
        jpeg_integrity
)

