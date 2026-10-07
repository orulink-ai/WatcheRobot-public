idf_component_register(
    SRCS "hx_ptl_protocol.c" "hx_ptl_transport.c" "hx_ptl_resync.c" "hx_vision_control_codec.c" "hx_photo_warmup.c"
    INCLUDE_DIRS "include"
    REQUIRES hx_media_protocol
)


