cmake_minimum_required(VERSION 3.20)
project(hx_media_protocol_host_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

add_executable(hx_media_protocol_host_tests
    "src/test_hx_media_protocol.c"
    "../../hx_media_protocol.c"
)

target_include_directories(hx_media_protocol_host_tests PRIVATE "../../include")

if(MSVC)
    target_compile_options(hx_media_protocol_host_tests PRIVATE /W4 /WX)
else()
    target_compile_options(hx_media_protocol_host_tests PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME hx_media_protocol_host_tests COMMAND hx_media_protocol_host_tests)


