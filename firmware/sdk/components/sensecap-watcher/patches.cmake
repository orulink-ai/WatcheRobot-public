# IDF6 触摸回归修复：在配置阶段对 managed_component espressif__esp_lcd_touch_spd2010
# 幂等打补丁（i2c_read 宏 cmd 0 -> -1，走 IDF6 纯读路径 i2c_master_receive）。
# 根因与修复方案见 scripts/patch_spd2010_idf6_touch.py 顶部注释。
#
# managed_component 在 .gitignore 里、clean rebuild 会被重置，故不能直接改源文件提交，
# 必须用配置阶段脚本持久化。脚本幂等：已打过补丁则跳过，driver 未拉取时也安全跳过。
#
# Python 解释器优先级：IDF 的 PYTHON 变量 > find_package(Python3) 缓存 > 退化到系统 python。
get_filename_component(_official_project "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT PROJECT_DIR STREQUAL _official_project)
    # Standalone projects own their managed dependencies. Never patch the
    # official application checkout while configuring a hardware example.
    set(_touch_source "${PROJECT_DIR}/managed_components/espressif__esp_lcd_touch_spd2010/esp_lcd_touch_spd2010.c")
    if(EXISTS "${_touch_source}")
        file(READ "${_touch_source}" _touch)
        string(REPLACE "esp_lcd_panel_io_rx_param(tp->io, 0, data_p, len)"
                       "esp_lcd_panel_io_rx_param(tp->io, -1, data_p, len)" _patched "${_touch}")
        if(NOT _patched STREQUAL _touch)
            file(WRITE "${_touch_source}" "${_patched}")
        endif()
    endif()
    set(_led_source "${PROJECT_DIR}/managed_components/espressif__led_strip/src/led_strip_spi_dev.c")
    if(EXISTS "${_led_source}")
        file(READ "${_led_source}" _led)
        if(NOT _led MATCHES "esp_heap_caps.h")
            file(WRITE "${_led_source}" "#include \"esp_heap_caps.h\"\n${_led}")
        endif()
    endif()
    return()
endif()
if(DEFINED PYTHON)
    set(_spd2010_py "${PYTHON}")
elseif(DEFINED _Python3_EXECUTABLE)
    set(_spd2010_py "${_Python3_EXECUTABLE}")
else()
    set(_spd2010_py "python")
endif()
execute_process(
    COMMAND "${_spd2010_py}" "${CMAKE_CURRENT_LIST_DIR}/../../scripts/patch_spd2010_idf6_touch.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE _spd2010_patch_out
    RESULT_VARIABLE _spd2010_patch_ret
)
string(STRIP "${_spd2010_patch_out}" _spd2010_patch_out)
message(STATUS "[sensecap-watcher] spd2010 touch patch: ${_spd2010_patch_out}")
if(_spd2010_patch_ret EQUAL 0)
    message(STATUS "[sensecap-watcher] spd2010 touch patch applied or already present")
else()
    message(WARNING "[sensecap-watcher] spd2010 touch patch failed (ret=${_spd2010_patch_ret}), touch may regress on clean rebuild")
endif()

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    execute_process(
        COMMAND "${_spd2010_py}" "${CMAKE_CURRENT_LIST_DIR}/../../scripts/patch_led_strip_idf6.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
        OUTPUT_VARIABLE _led_strip_patch_out
        RESULT_VARIABLE _led_strip_patch_ret
    )
    string(STRIP "${_led_strip_patch_out}" _led_strip_patch_out)
    message(STATUS "[sensecap-watcher] IDF6 led_strip patch: ${_led_strip_patch_out}")
    if(NOT _led_strip_patch_ret EQUAL 0)
        message(FATAL_ERROR "[sensecap-watcher] IDF6 led_strip patch failed (ret=${_led_strip_patch_ret})")
    endif()
endif()

# esp_capture 1.0.2 assumes configured video sink indexes are contiguous.
# WatcheRobot intentionally uses path 0 for RTC audio and path 1 for MJPEG,
# so apply the tracked, idempotent sparse-path fix after dependencies resolve.
execute_process(
    COMMAND "${_spd2010_py}" "${CMAKE_CURRENT_LIST_DIR}/../../scripts/patch_esp_capture_sparse_video_paths.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE _esp_capture_patch_out
    RESULT_VARIABLE _esp_capture_patch_ret
)
string(STRIP "${_esp_capture_patch_out}" _esp_capture_patch_out)
message(STATUS "[sensecap-watcher] esp_capture sparse video paths patch: ${_esp_capture_patch_out}")
if(NOT _esp_capture_patch_ret EQUAL 0)
    message(FATAL_ERROR
        "[sensecap-watcher] esp_capture sparse video paths patch failed "
        "(ret=${_esp_capture_patch_ret}); refusing a build that can deadlock RTC video startup"
    )
endif()

