cmake_minimum_required(VERSION 3.20)

project(mcu_sensor_service_host_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(MCU_SENSOR_COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(MCU_LINK_COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../protocols/mcu_link")

add_executable(mcu_sensor_service_host_tests
    "${MCU_SENSOR_COMPONENT_DIR}/src/mcu_sensor_service.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_mcu_sensor_service.c"
)

target_include_directories(mcu_sensor_service_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${MCU_SENSOR_COMPONENT_DIR}/include"
    "${MCU_LINK_COMPONENT_DIR}/include"
    "${MCU_LINK_COMPONENT_DIR}/test_support/host/include"
)

# Assertions are test actions as well as checks; keep them in Release.
if(MSVC)
    target_compile_options(mcu_sensor_service_host_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(mcu_sensor_service_host_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME mcu_sensor_service_host_tests COMMAND mcu_sensor_service_host_tests)

