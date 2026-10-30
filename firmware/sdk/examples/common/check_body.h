#pragma once
#include "check_flow.h"
#include "watcher_hw_body.h"

/* Controller-task adapter. Only SDK dispatch consumes MCU events. No LVGL or
 * media calls here; touch has no actuator policy. Times are monotonic us. */
typedef struct {
    hc_operation_t operation;
    unsigned argument, touch_stage;
    uint8_t touch_id, last_touch_id, last_touch_code;
    uint32_t attempt, sequence;
    int64_t started_us, deadline_us;
    int x, y;
    bool active, preparing, moving, finished, cancelled, stop_submitted, lights_owned;
    esp_err_t error, stop_error;
} hc_body_t;
void hc_body_init(hc_body_t *body);
esp_err_t hc_body_start(hc_body_t *body, hc_operation_t operation, unsigned argument, uint32_t attempt, int64_t now);
void hc_body_event(hc_body_t *body, const watcher_hw_event_t *event, int64_t now);
void hc_body_tick(hc_body_t *body, int64_t now);
void hc_body_cancel(hc_body_t *body);
esp_err_t hc_body_stop_test(hc_body_t *body);
esp_err_t hc_body_lights_off(hc_body_t *body);
