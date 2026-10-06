if(NOT CMAKE_BUILD_EARLY_EXPANSION AND NOT CONFIG_WATCHER_CAMERA_BACKEND_PTL)
    message(FATAL_ERROR "watche_hw_camera v1 requires WATCHER_CAMERA_BACKEND_PTL and paired Himax firmware")
endif()
idf_component_register(SRCS "watche_hw_camera.c" INCLUDE_DIRS "include" REQUIRES watche_hw_core hal_camera)

