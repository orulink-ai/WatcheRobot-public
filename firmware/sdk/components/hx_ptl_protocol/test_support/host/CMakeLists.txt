cmake_minimum_required(VERSION 3.20)
project(hx_ptl_protocol_host_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()
add_executable(hx_photo_warmup_tests src/test_photo_warmup.c ../../hx_photo_warmup.c)
target_include_directories(hx_photo_warmup_tests PRIVATE ../../include)
add_test(NAME hx_photo_warmup_tests COMMAND hx_photo_warmup_tests)
add_executable(hx_vision_control_codec_tests src/test_vision_control_codec.c ../../hx_vision_control_codec.c)
target_include_directories(hx_vision_control_codec_tests PRIVATE ../../include)
add_test(NAME hx_vision_control_codec_tests COMMAND hx_vision_control_codec_tests)
add_executable(hx_ptl_resync_tests src/test_hx_ptl_resync.c ../../hx_ptl_resync.c
    ../../hx_ptl_protocol.c ../../../hx_media_protocol/hx_media_protocol.c)
target_include_directories(hx_ptl_resync_tests PRIVATE ../../include ../../../hx_media_protocol/include)
if(MSVC)
    target_compile_options(hx_ptl_resync_tests PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(hx_ptl_resync_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME hx_ptl_resync_tests COMMAND hx_ptl_resync_tests)

add_executable(hx_ptl_protocol_host_tests
    "src/test_hx_ptl_protocol.c"
    "../../hx_ptl_protocol.c"
    "../../hx_ptl_transport.c"
    "../../../hx_media_protocol/hx_media_protocol.c"
)
target_include_directories(hx_ptl_protocol_host_tests PRIVATE
    "../../include"
    "../../../hx_media_protocol/include"
)
if(MSVC)
    target_compile_options(hx_ptl_protocol_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(hx_ptl_protocol_host_tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME hx_ptl_protocol_host_tests COMMAND hx_ptl_protocol_host_tests)

