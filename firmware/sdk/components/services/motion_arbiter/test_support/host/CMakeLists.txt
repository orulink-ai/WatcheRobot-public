cmake_minimum_required(VERSION 3.20)

project(motion_arbiter_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

set(COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(motion_arbiter_host_tests
    "${COMPONENT_DIR}/src/motion_arbiter.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_motion_arbiter.c"
)
target_include_directories(motion_arbiter_host_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${COMPONENT_DIR}/include"
)
target_compile_definitions(motion_arbiter_host_tests PRIVATE MOTION_ARBITER_HOST_TEST)
# Assertions are test actions as well as checks; keep them in Release.
if(MSVC)
    target_compile_options(motion_arbiter_host_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(motion_arbiter_host_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME motion_arbiter_host_tests COMMAND motion_arbiter_host_tests)

