idf_component_register(
    SRCS
        "src/hal_audio.c"
        "src/hal_opus.c"
    INCLUDE_DIRS "include"
    REQUIRES
        "espressif__esp_codec_dev"
    PRIV_REQUIRES
        "sensecap-watcher"
        "esp_audio_codec"
)

