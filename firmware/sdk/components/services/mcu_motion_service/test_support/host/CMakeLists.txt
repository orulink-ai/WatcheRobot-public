cmake_minimum_required(VERSION 3.20)

project(mcu_motion_service_host_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(MCU_MOTION_COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(mcu_motion_service_host_tests
    "${MCU_MOTION_COMPONENT_DIR}/src/mcu_motion_service.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_mcu_motion_service.c"
)

target_include_directories(mcu_motion_service_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${MCU_MOTION_COMPONENT_DIR}/include"
)

target_compile_definitions(mcu_motion_service_host_tests PRIVATE MCU_MOTION_HOST_TEST=1)

# Assertions are test actions as well as checks; keep them in Release.
if(MSVC)
    target_compile_options(mcu_motion_service_host_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(mcu_motion_service_host_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME mcu_motion_service_host_tests COMMAND mcu_motion_service_host_tests)

