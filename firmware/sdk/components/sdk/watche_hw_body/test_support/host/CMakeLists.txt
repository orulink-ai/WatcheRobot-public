cmake_minimum_required(VERSION 3.20)
project(watche_body_host C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()
add_executable(body_test test_body.c ../../watche_hw_body.c ../../../watche_hw_core/watche_hw_core.c)
target_include_directories(body_test PRIVATE include ../../include ../../../watche_hw_core/include
  ../../../../protocols/mcu_link/test_support/host/include ../../../../protocols/mcu_link/include
  ../../../../services/mcu_runtime/include ../../../../services/mcu_motion_service/include
  ../../../../services/mcu_led_service/include ../../../../services/mcu_sensor_service/include
  ../../../../services/mcu_power_service/include ../../../../services/motion_arbiter/include)
if(MSVC)
  target_compile_options(body_test PRIVATE /W4 /UNDEBUG)
else()
  target_compile_options(body_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME body_runtime COMMAND body_test)

