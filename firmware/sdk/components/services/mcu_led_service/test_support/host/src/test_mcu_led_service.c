#include "mcu_led_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t msg_class;
    uint8_t msg_id;
    uint8_t flags;
    uint8_t payload[128];
    uint16_t payload_len;
    uint32_t seq;
    size_t wire_len;
    unsigned send_count;
} captured_frame_t;

static mcu_link_t s_link = {0};
static bool s_link_ready = true;
static bool s_ready = true;
static captured_frame_t s_captured;

mcu_link_t *mcu_link_bootstrap_get_link(void) {
    return &s_link;
}

bool mcu_link_bootstrap_is_ready(void) {
    return s_ready;
}

bool mcu_link_bootstrap_is_link_ready(void) {
    return s_link_ready;
}

esp_err_t mcu_link_send_frame(mcu_link_t *link, uint8_t msg_class, uint8_t msg_id, uint8_t flags,
                              const uint8_t *payload, uint16_t payload_len, uint32_t *out_seq, size_t *out_wire_len) {
    assert(link == &s_link);
    assert(payload_len <= sizeof(s_captured.payload));

    s_captured.msg_class = msg_class;
    s_captured.msg_id = msg_id;
    s_captured.flags = flags;
    s_captured.payload_len = payload_len;
    if (payload_len > 0u) {
        assert(payload != NULL);
        memcpy(s_captured.payload, payload, payload_len);
    }
    s_captured.seq = 42u;
    s_captured.wire_len = payload_len + 8u;
    s_captured.send_count++;

    if (out_seq != NULL) {
        *out_seq = s_captured.seq;
    }
    if (out_wire_len != NULL) {
        *out_wire_len = s_captured.wire_len;
    }
    return ESP_OK;
}

static void reset_capture(void) {
    memset(&s_captured, 0, sizeof(s_captured));
    s_link_ready = true;
    s_ready = true;
    assert(mcu_led_service_init() == ESP_OK);
}

static void test_boot_green_baseline_sends_static_green(void) {
    mcu_led_request_t last_request = {0};

    reset_capture();

    assert(mcu_led_submit_boot_green_baseline() == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_class == MCU_FRAME_CLASS_LED);
    assert(s_captured.msg_id == MCU_LED_MSG_SET_STATIC);
    assert(s_captured.flags == MCU_FRAME_FLAG_ACK_REQ);
    assert(s_captured.payload_len == 5u);
    assert(s_captured.payload[0] == 0u);
    assert(s_captured.payload[1] == 255u);
    assert(s_captured.payload[2] == 0u);
    assert(s_captured.payload[3] == 4u);
    assert(s_captured.payload[4] == 0u);

    assert(mcu_led_service_get_last_request(&last_request) == ESP_OK);
    assert(last_request.mode == MCU_LED_MODE_STATIC);
    assert(last_request.zone == MCU_LED_ZONE_ALL);
    assert(last_request.primary_red == 0u);
    assert(last_request.primary_green == 255u);
    assert(last_request.primary_blue == 0u);
    assert(last_request.brightness == 255u);
    assert(last_request.period_ms == 0u);
}

static void test_boot_green_baseline_requires_ready_link(void) {
    reset_capture();
    s_ready = false;

    assert(mcu_led_submit_boot_green_baseline() == ESP_ERR_INVALID_STATE);
    assert(s_captured.send_count == 0u);
}

static void test_static_light_sends_target_zone_and_scaled_brightness(void) {
    const mcu_led_request_t request = {
        .mode = MCU_LED_MODE_STATIC,
        .zone = MCU_LED_ZONE_BOTTOM,
        .primary_red = 0u,
        .primary_green = 200u,
        .primary_blue = 100u,
        .brightness = 128u,
    };

    reset_capture();

    assert(mcu_led_submit(&request) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_class == MCU_FRAME_CLASS_LED);
    assert(s_captured.msg_id == MCU_LED_MSG_SET_STATIC);
    assert(s_captured.payload_len == 5u);
    assert(s_captured.payload[0] == 0u);
    assert(s_captured.payload[1] == 100u);
    assert(s_captured.payload[2] == 50u);
    assert(s_captured.payload[3] == 4u);
    assert(s_captured.payload[4] == MCU_LED_ZONE_BOTTOM);
}

static void test_off_light_sends_target_zone(void) {
    const mcu_led_request_t request = {
        .mode = MCU_LED_MODE_OFF,
        .zone = MCU_LED_ZONE_SIDE,
    };

    reset_capture();

    assert(mcu_led_submit(&request) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_class == MCU_FRAME_CLASS_LED);
    assert(s_captured.msg_id == MCU_LED_MSG_OFF);
    assert(s_captured.payload_len == 1u);
    assert(s_captured.payload[0] == MCU_LED_ZONE_SIDE);
}

static void test_off_light_is_allowed_after_handshake_before_baseline_sync(void) {
    const mcu_led_request_t request = {
        .mode = MCU_LED_MODE_OFF,
        .zone = MCU_LED_ZONE_ALL,
    };

    reset_capture();
    s_ready = false;

    assert(mcu_led_submit(&request) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_id == MCU_LED_MSG_OFF);
}

static void test_off_light_is_allowed_before_handshake_as_safe_fallback(void) {
    const mcu_led_request_t request = {
        .mode = MCU_LED_MODE_OFF,
        .zone = MCU_LED_ZONE_ALL,
    };

    reset_capture();
    s_link_ready = false;
    s_ready = false;

    assert(mcu_led_submit(&request) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_id == MCU_LED_MSG_OFF);
}

static void test_shutdown_mode_rejects_relight_but_allows_led_off(void) {
    const mcu_led_request_t static_request = {
        .mode = MCU_LED_MODE_STATIC,
        .zone = MCU_LED_ZONE_ALL,
        .primary_red = 255u,
        .brightness = 255u,
    };
    const mcu_led_request_t effect_request = {
        .mode = MCU_LED_MODE_EFFECT,
        .zone = MCU_LED_ZONE_ALL,
        .primary_blue = 255u,
        .brightness = 255u,
        .effect_id = MCU_LED_EFFECT_BLINK,
        .period_ms = 500u,
    };
    const mcu_led_request_t off_request = {
        .mode = MCU_LED_MODE_OFF,
        .zone = MCU_LED_ZONE_ALL,
    };

    reset_capture();

    assert(mcu_led_service_enter_shutdown_mode() == ESP_OK);
    assert(mcu_led_submit(&static_request) == ESP_ERR_INVALID_STATE);
    assert(mcu_led_submit(&effect_request) == ESP_ERR_INVALID_STATE);
    assert(s_captured.send_count == 0u);
    assert(mcu_led_submit(&off_request) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_id == MCU_LED_MSG_OFF);
}

static void test_effect_light_preserves_effect_identity_on_wire(void) {
    static const mcu_led_effect_t effects[] = {
        MCU_LED_EFFECT_BLINK,
        MCU_LED_EFFECT_BREATHING,
        MCU_LED_EFFECT_RAINBOW,
        MCU_LED_EFFECT_STATUS_PULSE,
    };
    size_t index;

    for (index = 0u; index < sizeof(effects) / sizeof(effects[0]); ++index) {
        const mcu_led_request_t request = {
            .mode = MCU_LED_MODE_EFFECT,
            .zone = MCU_LED_ZONE_ALL,
            .primary_red = 10u,
            .primary_green = 20u,
            .primary_blue = 30u,
            .brightness = 255u,
            .effect_id = effects[index],
            .period_ms = 640u,
        };

        reset_capture();
        assert(mcu_led_submit(&request) == ESP_OK);
        assert(s_captured.send_count == 1u);
        assert(s_captured.msg_id == MCU_LED_MSG_SET_EFFECT);
        assert(s_captured.payload_len == 7u);
        assert(s_captured.payload[0] == effects[index]);
        assert(s_captured.payload[1] == 10u);
        assert(s_captured.payload[2] == 20u);
        assert(s_captured.payload[3] == 30u);
        assert(s_captured.payload[4] == 0x80u);
        assert(s_captured.payload[5] == 0x02u);
        assert(s_captured.payload[6] == MCU_LED_ZONE_ALL);
    }
}

int main(void) {
    test_boot_green_baseline_sends_static_green();
    test_boot_green_baseline_requires_ready_link();
    test_static_light_sends_target_zone_and_scaled_brightness();
    test_off_light_sends_target_zone();
    test_off_light_is_allowed_after_handshake_before_baseline_sync();
    test_off_light_is_allowed_before_handshake_as_safe_fallback();
    test_shutdown_mode_rejects_relight_but_allows_led_off();
    test_effect_light_preserves_effect_identity_on_wire();
    return 0;
}

