#include "watche_hw_body.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
static bool position_pending;
static int64_t position_requested;
static void on_body_event(const watche_hw_event_t *received, void *context) {
    (void)context;
    watche_hw_event_t event = *received;
        ESP_LOGI("example", "event=%d seq=%lu", event.type, (unsigned long)event.sequence);
        if (event.type == WATCHE_HW_EVENT_TOUCH && event.size == sizeof(mcu_touch_state_t)) {
            mcu_touch_state_t touch;
            memcpy(&touch, event.payload, sizeof(touch));
            ESP_LOGI("example", "touch=%u code=%d", touch.touch_id, touch.event_code);
            mcu_led_request_t light = {.mode=MCU_LED_MODE_STATIC, .zone=MCU_LED_ZONE_ALL,
                .primary_green=touch.active ? 128 : 0, .brightness=64};
            ESP_LOGI("example", "light: %s", esp_err_to_name(watche_hw_body_light(&light)));
            /* Query fresh feedback first; never move blindly to a fixed pose. */
            if (touch.event_code == MCU_TOUCH_EVENT_LONG_PRESS) {
                position_requested = esp_timer_get_time();
                position_pending = watche_hw_body_request_position() == ESP_OK;
            } else if (touch.event_code == MCU_TOUCH_EVENT_RELEASE) {
                position_pending = false;
                ESP_LOGI("example", "stop: %s", esp_err_to_name(watche_hw_body_stop()));
            }
        } else if (event.type == WATCHE_HW_EVENT_LINK) {
            position_pending = false; /* A reconnect never replays an action. */
        } else if (event.type == WATCHE_HW_EVENT_POSITION &&
                   event.size == sizeof(mcu_motion_servo_feedback_t)) {
            mcu_motion_servo_feedback_t position;
            memcpy(&position, event.payload, sizeof(position));
            bool fresh = position_pending && event.received_us >= position_requested &&
                esp_timer_get_time() - event.received_us < 500000 &&
                event.received_us - position_requested < 500000;
            position_pending = false;
            if (fresh && position.axis_mask == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y) &&
                position.x_angle_x10 >= 0 && position.x_angle_x10 <= 1800 &&
                position.y_angle_x10 >= 1000 && position.y_angle_x10 <= 1400) {
                int x = (position.x_angle_x10 + 5) / 10;
                int y = (position.y_angle_x10 + 5) / 10;
                x += x <= 175 ? 3 : -3; /* At most 3.5 degrees including rounding. */
                uint32_t sequence;
                ESP_LOGI("example", "small move: %s", esp_err_to_name(watche_hw_body_move(x, y, 1000, &sequence)));
            } else {
                ESP_LOGW("example", "No fresh valid position; movement skipped");
            }
        } else if (event.type == WATCHE_HW_EVENT_MOTION && event.size == sizeof(mcu_motion_lifecycle_event_t)) {
            mcu_motion_lifecycle_event_t result;
            memcpy(&result, event.payload, sizeof(result));
            static const char *names[] = {"ACKED", "REJECTED", "DONE", "FAULT"};
            const char *name = (unsigned)result.type < 4 ? names[result.type] : "UNKNOWN";
            ESP_LOGI("motion", "%s seq=%lu result=%d reason=%u fault=%u final_x10=(%d,%d) ms=%u",
                     name, (unsigned long)result.ref_seq, result.result, result.reason,
                     result.fault_code, result.final_x_deg_x10, result.final_y_deg_x10, result.exec_ms);
        }
}
static void body_events(void) {
    uint32_t dispatched;
    (void)watche_hw_body_dispatch_events(32, &dispatched);
    uint32_t dropped;
    if (watche_hw_body_dropped_events(&dropped) == ESP_OK && dropped)
        ESP_LOGW("example", "dropped events: %lu", (unsigned long)dropped);
}
void app_main(void) {
    ESP_LOGI("example", "body: %s", esp_err_to_name(watche_hw_body_init()));
    ESP_LOGI("example", "subscribe: %s", esp_err_to_name(watche_hw_body_subscribe(on_body_event, NULL)));
    for (;;) {
        body_events();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

