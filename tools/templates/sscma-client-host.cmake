cmake_minimum_required(VERSION 3.16)
project(sscma_client_command_host_tests C)

enable_testing()

set(CJSON_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../managed_components/espressif__cjson/cJSON"
    CACHE PATH "Directory containing cJSON.c and cJSON.h (tested with cJSON v1.7.19)")
if(NOT EXISTS "${CJSON_SOURCE_DIR}/cJSON.c" OR NOT EXISTS "${CJSON_SOURCE_DIR}/cJSON.h")
    message(FATAL_ERROR
        "cJSON sources not found in '${CJSON_SOURCE_DIR}'. Set -DCJSON_SOURCE_DIR=/absolute/path/to/cJSON "
        "to a directory containing cJSON.c and cJSON.h (e.g. upstream v1.7.19).")
endif()

add_executable(
    sscma_client_command_host_tests
    ../../src/sscma_client_command.c
    ../../src/sscma_client_sensor_parser.c
    "${CJSON_SOURCE_DIR}/cJSON.c"
    src/test_sscma_client_command.c
)
target_include_directories(
    sscma_client_command_host_tests
    PRIVATE
    ../../include
    "${CJSON_SOURCE_DIR}"
)

# The test invokes functions inside assert(); keep checks active in Release too.
if(MSVC)
    target_compile_options(sscma_client_command_host_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(sscma_client_command_host_tests PRIVATE -UNDEBUG)
endif()

add_test(NAME sscma_client_command_host_tests COMMAND sscma_client_command_host_tests)
