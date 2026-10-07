cmake_minimum_required(VERSION 3.20)
project(watche_hw_core_tests C)
set(CMAKE_C_STANDARD 11)
enable_testing()
add_executable(watche_hw_core_tests ../../watche_hw_core.c test_core.c)
target_include_directories(watche_hw_core_tests PRIVATE include ../../include)
# Keep assertions active for Release as well as Debug.
if(MSVC)
    target_compile_options(watche_hw_core_tests PRIVATE /UNDEBUG /W4)
else()
    target_compile_options(watche_hw_core_tests PRIVATE -UNDEBUG -Wall -Wextra -Werror)
endif()
add_test(NAME watche_hw_core_tests COMMAND watche_hw_core_tests)

