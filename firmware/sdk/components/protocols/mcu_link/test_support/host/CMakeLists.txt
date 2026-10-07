cmake_minimum_required(VERSION 3.20)

project(mcu_link_host_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(MCU_LINK_COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(MCU_LINK_TEST_SUPPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(mcu_link_host_tests
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_crc16.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_cobs.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_frame.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_wire.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_link_fsm.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_link_stats.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_link.c"
    "${MCU_LINK_COMPONENT_DIR}/src/mcu_link_bootstrap.c"
    "${MCU_LINK_TEST_SUPPORT_DIR}/src/mcu_link_test_support.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_mcu_link_poll.c"
)

target_include_directories(mcu_link_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${MCU_LINK_COMPONENT_DIR}/include"
    "${MCU_LINK_TEST_SUPPORT_DIR}/include"
)

# Assertions are test actions as well as checks; keep them in Release.
if(MSVC)
    target_compile_options(mcu_link_host_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(mcu_link_host_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME mcu_link_host_tests COMMAND mcu_link_host_tests)

# Keep real-driver concurrency coverage in the standard host/CI entry point.
add_subdirectory("${MCU_LINK_TEST_SUPPORT_DIR}/uart_host" uart_gate)

