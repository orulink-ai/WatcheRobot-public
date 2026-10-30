#include "check_body.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static watcher_hw_status_t status = {WATCHER_HW_READY, ESP_OK};
static unsigned moves, stops, queries, leds;
static int target_x, target_y;
static esp_err_t stop_error;
esp_err_t watcher_hw_body_status(watcher_hw_status_t *out) {
    *out = status;
    return ESP_OK;
}
esp_err_t watcher_hw_body_request_position(void) {
    queries++;
    return ESP_OK;
}
esp_err_t watcher_hw_body_move(int x, int y, uint32_t duration, uint32_t *seq) {
    assert(duration == 1000 || duration == 3000);
    moves++;
    target_x = x;
    target_y = y;
    *seq = 76 + moves;
    return ESP_OK;
}
esp_err_t watcher_hw_body_stop(void) {
    stops++;
    return stop_error;
}
esp_err_t watcher_hw_body_light(const mcu_led_request_t *request) {
    assert(request->zone <= MCU_LED_ZONE_BOTTOM);
    leds++;
    return ESP_OK;
}
static watcher_hw_event_t event(watcher_hw_event_type_t type, const void *data, size_t size, int64_t us) {
    watcher_hw_event_t e = {.type = type, .received_us = us, .size = size};
    assert(size <= sizeof(e.payload));
    if (data)
        memcpy(e.payload, data, size);
    return e;
}
static void position(hc_body_t *b, int x, int y, int64_t received, int64_t now) {
    mcu_motion_servo_feedback_t p = {.axis_mask = 3, .x_angle_x10 = (int16_t)x, .y_angle_x10 = (int16_t)y};
    watcher_hw_event_t e = event(WATCHER_HW_EVENT_POSITION, &p, sizeof(p), received);
    hc_body_event(b, &e, now);
}
static void touch(hc_body_t *b, unsigned id, mcu_touch_event_code_t code, int64_t us) {
    mcu_touch_state_t t = {.touch_id = (uint8_t)id, .event_code = code};
    watcher_hw_event_t e = event(WATCHER_HW_EVENT_TOUCH, &t, sizeof(t), us);
    hc_body_event(b, &e, us);
}
static void lifecycle(hc_body_t *b, mcu_motion_lifecycle_event_type_t type, uint32_t seq, mcu_motion_result_t result) {
    mcu_motion_lifecycle_event_t m = {.type = type, .ref_seq = seq, .result = result};
    watcher_hw_event_t e = event(WATCHER_HW_EVENT_MOTION, &m, sizeof(m), 2000);
    hc_body_event(b, &e, 2000);
}
int main(void) {
    hc_body_t b;
    hc_body_init(&b);
    assert(moves == 0 && leds == 0 && stops == 0);
    assert(hc_body_start(&b, HC_OP_TOUCH, 0, 1, 1000) == ESP_OK);
    touch(&b, 0, MCU_TOUCH_EVENT_RELEASE, 1100);
    touch(&b, 0, MCU_TOUCH_EVENT_LONG_PRESS, 1200);
    assert(!b.finished && moves == 0 && leds == 0 && queries == 0);
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1300);
    touch(&b, 1, MCU_TOUCH_EVENT_RELEASE, 1400); /* Different sensor cannot finish gesture. */
    assert(b.touch_stage == 0);
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1450);
    touch(&b, 0, MCU_TOUCH_EVENT_RELEASE, 1500);
    assert(b.touch_stage == 2);
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1600);
    touch(&b, 0, MCU_TOUCH_EVENT_LONG_PRESS, 1700);
    touch(&b, 0, MCU_TOUCH_EVENT_RELEASE, 1800);
    assert(b.finished && b.error == ESP_OK && moves == 0 && leds == 0);
    assert(hc_body_start(&b, HC_OP_TOUCH, 0, 1, 1000) == ESP_OK);
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1300);
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1400);
    assert(b.touch_stage == 0); /* Duplicate/misordered protocol event breaks proof. */
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1500);
    touch(&b, 0, MCU_TOUCH_EVENT_RELEASE, 1600);
    touch(&b, 0, MCU_TOUCH_EVENT_PRESS, 1700);
    touch(&b, 0, MCU_TOUCH_EVENT_RELEASE, 1800); /* Too short, not a hold. */
    touch(&b, 0, MCU_TOUCH_EVENT_LONG_PRESS, 1900);
    assert(b.touch_stage == 0 && !b.finished);
    hc_body_cancel(&b);

    /* Open-loop acceptance uses an explicit reference command, never ADC. */
    assert(hc_body_start(&b, HC_OP_MOVE, 0, 2, 1000) == ESP_OK);
    assert(queries == 0 && moves == 1 && target_x == 90 && target_y == 120);
    assert(b.moving);
    uint32_t reference = b.sequence;
    position(&b, 1798, 1398, 1200, 1300);
    assert(moves == 1); /* Legacy feedback cannot affect either target. */
    lifecycle(&b, MCU_MOTION_LIFECYCLE_ACKED, reference, MCU_MOTION_RESULT_SUCCESS);
    assert(moves == 1 && !b.finished);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, reference - 1, MCU_MOTION_RESULT_SUCCESS);
    assert(moves == 1);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, reference, MCU_MOTION_RESULT_SUCCESS);
    assert(moves == 2 && target_x == 93 && target_y == 120);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, reference, MCU_MOTION_RESULT_SUCCESS);
    assert(!b.finished); /* Delayed reference completion is not step completion. */
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    assert(b.finished && b.error == ESP_OK);

    assert(hc_body_start(&b, HC_OP_MOVE, 1, 3, 1000) == ESP_OK);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    assert(target_x == 90 && target_y == 123);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_REJECTED, b.sequence, MCU_MOTION_RESULT_FAULT);
    assert(b.finished && b.error != ESP_OK);

    assert(hc_body_start(&b, HC_OP_STOP_TEST, 0, 4, 1000) == ESP_OK);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    assert(hc_body_stop_test(&b) == ESP_OK && !b.finished);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence - 1, MCU_MOTION_RESULT_STOPPED);
    assert(!b.finished);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_STOPPED);
    assert(b.finished && b.error == ESP_OK);
    assert(hc_body_start(&b, HC_OP_STOP_TEST, 0, 5, 1000) == ESP_OK);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    assert(b.finished && b.error != ESP_OK); /* Natural completion cannot pass STOP. */

    assert(hc_body_start(&b, HC_OP_MOVE, 0, 6, 1000) == ESP_OK);
    unsigned previous_moves = moves;
    hc_body_cancel(&b);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_STOPPED);
    assert(b.finished && b.cancelled && moves == previous_moves);
    assert(hc_body_start(&b, HC_OP_MOVE, 0, 7, 1000) == ESP_OK);
    previous_moves = moves;
    watcher_hw_event_t link = event(WATCHER_HW_EVENT_LINK, NULL, 0, 1200);
    hc_body_event(&b, &link, 1300);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    position(&b, 900, 1200, 1400, 1500);
    assert(b.finished && b.error != ESP_OK && moves == previous_moves);

    assert(hc_body_start(&b, HC_OP_MOVE, 0, 8, 1000) == ESP_OK);
    watcher_hw_event_t lost = event(WATCHER_HW_EVENT_OVERFLOW, NULL, 0, 1200);
    hc_body_event(&b, &lost, 1300);
    lifecycle(&b, MCU_MOTION_LIFECYCLE_DONE, b.sequence, MCU_MOTION_RESULT_SUCCESS);
    assert(b.finished && b.error == ESP_ERR_NO_MEM);
    assert(hc_body_start(&b, HC_OP_MOVE, 0, 9, 1000) == ESP_OK);
    hc_body_tick(&b, 6000000);
    assert(b.finished && b.error == ESP_ERR_TIMEOUT);
    assert(hc_body_start(&b, HC_OP_MOVE, 0, 10, 1000) == ESP_OK);
    stop_error = ESP_ERR_TIMEOUT;
    hc_body_cancel(&b);
    assert(b.finished && b.error == ESP_ERR_TIMEOUT);
    stop_error = ESP_OK;
    status.state = WATCHER_HW_STARTING;
    assert(hc_body_start(&b, HC_OP_MOVE, 0, 11, 1000) == ESP_ERR_INVALID_STATE);
    status.state = WATCHER_HW_READY;
    for (unsigned i = 0; i < 14; i++)
        assert(hc_body_start(&b, HC_OP_LIGHT, i, 12, 1000) == ESP_OK);
    assert(leds == 28 && b.lights_owned);
    assert(hc_body_lights_off(&b) == ESP_OK && !b.lights_owned);
    assert(queries == 0);
    puts("open-loop body adapter: reference, explicit steps, sequence matching, stop, disconnect passed");
    return 0;
}
