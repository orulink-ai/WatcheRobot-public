cmake_minimum_required(VERSION 3.16)
project(sscma_client_command_host_tests C)

enable_testing()

add_executable(
    sscma_client_command_host_tests
    ../../src/sscma_client_command.c
    ../../src/sscma_client_sensor_parser.c
    ../../../../managed_components/espressif__cjson/cJSON/cJSON.c
    src/test_sscma_client_command.c
)
target_include_directories(
    sscma_client_command_host_tests
    PRIVATE
    ../../include
    ../../../../managed_components/espressif__cjson/cJSON
)

add_test(NAME sscma_client_command_host_tests COMMAND sscma_client_command_host_tests)

