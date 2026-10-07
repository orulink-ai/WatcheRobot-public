#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "watche_hw_body.h"
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
        mcu_led_request_t light = {.mode = MCU_LED_MODE_STATIC,
                                   .zone = MCU_LED_ZONE_ALL,
                                   .primary_green = touch.active ? 128 : 0,
                                   .brightness = 64};
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
    } else if (event.type == WATCHE_HW_EVENT_POSITION && event.size == sizeof(mcu_motion_servo_feedback_t)) {
        mcu_motion_servo_feedback_t position;
        memcpy(&position, event.payload, sizeof(position));
        bool fresh = position_pending && event.received_us >= position_requested &&
                     esp_timer_get_time() - event.received_us < 500000 &&
                     event.received_us - position_requested < 500000;
        position_pending = false;
        if (fresh && position.axis_mask == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y) && position.x_angle_x10 >= 0 &&
            position.x_angle_x10 <= 1800 && position.y_angle_x10 >= 1000 && position.y_angle_x10 <= 1400) {
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
        ESP_LOGI("motion", "%s seq=%lu result=%d reason=%u fault=%u final_x10=(%d,%d) ms=%u", name,
                 (unsigned long)result.ref_seq, result.result, result.reason, result.fault_code, result.final_x_deg_x10,
                 result.final_y_deg_x10, result.exec_ms);
    }
}
static void body_events(void) {
    uint32_t dispatched;
    (void)watche_hw_body_dispatch_events(32, &dispatched);
    uint32_t dropped;
    if (watche_hw_body_dropped_events(&dropped) == ESP_OK && dropped)
        ESP_LOGW("example", "dropped events: %lu", (unsigned long)dropped);
}
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "watche_hw_audio.h"
#include "watche_hw_camera.h"
#include "watche_hw_display.h"
static QueueHandle_t actions;
static int16_t pcm[8000]; /* half a second at 16 kHz; no SD/model resources */
static void frame(const uint8_t *jpeg, size_t size, uint32_t timestamp, void *ctx) {
    (void)ctx;
    bool valid = size >= 4 && jpeg[0] == 0xff && jpeg[1] == 0xd8 && jpeg[size - 2] == 0xff && jpeg[size - 1] == 0xd9;
    ESP_LOGI("jpeg", "bytes=%u timestamp=%lu markers=%s", (unsigned)size, (unsigned long)timestamp,
             valid ? "OK" : "INVALID");
    /* Borrowed bytes; copy here if your own task needs the image. */
}
static void clicked(lv_event_t *event) {
    int action = (int)(intptr_t)lv_event_get_user_data(event);
    (void)xQueueSend(actions, &action, 0); /* Never block the LVGL task. */
}
static void head_init(void) {
    actions = xQueueCreate(4, sizeof(int));
    if (!actions)
        return;
    ESP_LOGI("example", "display: %s", esp_err_to_name(watche_hw_display_init()));
    if (watche_hw_display_lock(1000)) {
        const char *labels[] = {"Photo", "Record / play"};
        for (int i = 0; i < 2; ++i) {
            lv_obj_t *button = lv_btn_create(lv_scr_act());
            lv_obj_set_size(button, 180, 60);
            lv_obj_align(button, LV_ALIGN_CENTER, 0, i ? 45 : -45);
            lv_obj_t *label = lv_label_create(button);
            lv_label_set_text(label, labels[i]);
            lv_obj_center(label);
            lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
        }
        watche_hw_display_unlock();
    }
    ESP_LOGI("example", "camera: %s", esp_err_to_name(watche_hw_camera_init()));
    ESP_LOGI("example", "audio: %s", esp_err_to_name(watche_hw_audio_init(16000, 30)));
}
static void head_actions(void) {
    int action;
    if (!actions || xQueueReceive(actions, &action, 0) != pdTRUE)
        return;
    if (action == 1)
        ESP_LOGI("example", "photo: %s", esp_err_to_name(watche_hw_camera_capture(frame, NULL)));
    else {
        esp_err_t ret = watche_hw_audio_read(pcm, sizeof(pcm));
        if (ret == ESP_OK)
            ret = watche_hw_audio_write(pcm, sizeof(pcm));
        ESP_LOGI("example", "PCM record/play: %s", esp_err_to_name(ret));
    }
}
void app_main(void) {
    ESP_LOGI("example", "body: %s", esp_err_to_name(watche_hw_body_init()));
    ESP_LOGI("example", "subscribe: %s", esp_err_to_name(watche_hw_body_subscribe(on_body_event, NULL)));
    head_init();
    for (;;) {
        body_events();
        head_actions();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
