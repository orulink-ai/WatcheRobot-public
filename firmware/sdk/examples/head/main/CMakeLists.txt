idf_component_register(SRCS "app_main.c" "../../common/head_demo.c" "../../common/demo_state.c" "../../common/demo_audio.c"
    "../../common/demo_playback.c" "../../common/demo_speech.c" "../../common/demo_image.c" "../../common/demo_record.c"
    INCLUDE_DIRS "." "../../common"
    REQUIRES watcher_hw_camera watcher_hw_audio watcher_hw_display espressif__esp_jpeg)
