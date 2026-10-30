idf_component_register(SRCS "app_main.c" "check_app.c" "check_view.c" "check_layout.c" "check_media.c"
    "../../common/check_flow.c" "../../common/check_body.c"
    "../../common/demo_image.c" "../../common/demo_state.c" "../../common/demo_record.c"
    "../../common/demo_audio.c" "../../common/demo_playback.c" "../../common/demo_speech.c"
    INCLUDE_DIRS "." "../../common"
    REQUIRES watcher_hw_body watcher_hw_camera watcher_hw_audio watcher_hw_display
        espressif__esp_jpeg esp_timer)

if(NOT CONFIG_LV_FONT_MONTSERRAT_18 OR NOT CONFIG_LV_FONT_MONTSERRAT_20)
    message(FATAL_ERROR "Hardware check v2 needs Montserrat 18 and 20. Enable both in idf.py menuconfig (LVGL font usage); new sdkconfig.defaults already enables them.")
endif()
