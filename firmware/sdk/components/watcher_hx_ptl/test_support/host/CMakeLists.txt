cmake_minimum_required(VERSION 3.20)
project(hx_control_host_tests C)
enable_testing()
add_executable(hx_control_tests test_control.c ../../watcher_hx_control.c ../../../hx_ptl_protocol/hx_vision_control_codec.c)
target_include_directories(hx_control_tests PRIVATE stubs ../../include ../../../hx_ptl_protocol/include)
add_test(NAME hx_control_tests COMMAND hx_control_tests)

