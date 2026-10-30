#include "mcu_sensor_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static void write_u32_le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16u) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static mcu_link_event_t make_touch_event(uint8_t touch_id, mcu_touch_event_code_t code, uint32_t timestamp_ms) {
    mcu_link_event_t event = {0};

    event.type = MCU_LINK_RX_EVENT_TOUCH_EVENT;
    event.frame.header.msg_class = MCU_FRAME_CLASS_SENSOR;
    event.frame.header.msg_id = MCU_SENSOR_MSG_TOUCH_EVENT;
    event.frame.header.payload_len = 6u;
    event.frame.payload[0] = touch_id;
    event.frame.payload[1] = (uint8_t)code;
    write_u32_le(&event.frame.payload[2], timestamp_ms);

    return event;
}

static void expect_touch(uint8_t touch_id, mcu_touch_event_code_t code, bool active, uint32_t timestamp_ms) {
    const mcu_link_event_t event = make_touch_event(touch_id, code, timestamp_ms);
    mcu_touch_state_t state = {0};
    bool overwrote_latest = true;

    assert(mcu_sensor_service_init() == ESP_OK);
    assert(mcu_sensor_service_handle_link_event(&event, &overwrote_latest) == ESP_OK);
    assert(!overwrote_latest);
    assert(mcu_sensor_service_has_latest_touch());
    assert(mcu_sensor_service_get_latest_touch(&state) == ESP_OK);
    assert(state.touch_id == touch_id);
    assert(state.event_code == code);
    assert(state.active == active);
    assert(state.timestamp_ms == timestamp_ms);
}

static void test_touch_press_is_active(void) {
    expect_touch(0u, MCU_TOUCH_EVENT_PRESS, true, 123456u);
}

static void test_touch_release_is_inactive(void) {
    expect_touch(0u, MCU_TOUCH_EVENT_RELEASE, false, 123789u);
}

static void test_touch_long_press_is_active(void) {
    expect_touch(0u, MCU_TOUCH_EVENT_LONG_PRESS, true, 124000u);
}

int main(void) {
    const struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        {"touch_press_is_active", test_touch_press_is_active},
        {"touch_release_is_inactive", test_touch_release_is_inactive},
        {"touch_long_press_is_active", test_touch_long_press_is_active},
    };
    size_t i;

    for (i = 0u; i < ARRAY_SIZE(tests); ++i) {
        tests[i].fn();
        printf("[PASS] %s\n", tests[i].name);
    }

    return 0;
}
