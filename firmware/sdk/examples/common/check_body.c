#include "check_body.h"
#include <string.h>

static bool ready(void) {
    watcher_hw_status_t s;
    return watcher_hw_body_status(&s) == ESP_OK && s.state == WATCHER_HW_READY;
}
static void finish(hc_body_t *b, esp_err_t error) {
    if (b->error == ESP_OK)
        b->error = error;
    b->active = false;
    b->preparing = false;
    b->moving = false;
    b->finished = true;
}
static esp_err_t stop(hc_body_t *b) {
    /* SDK permits stop even when its event-loss latch rejects new controls.
     * A disconnected transport rejects this; never promise physical stop. */
    b->stop_error = watcher_hw_body_stop();
    b->stop_submitted = b->stop_error == ESP_OK;
    return b->stop_error;
}
void hc_body_init(hc_body_t *b) {
    memset(b, 0, sizeof(*b));
}
esp_err_t hc_body_lights_off(hc_body_t *b) {
    if (!b->lights_owned)
        return ESP_OK;
    const mcu_led_request_t off = {.mode = MCU_LED_MODE_OFF, .zone = MCU_LED_ZONE_ALL};
    esp_err_t ret = watcher_hw_body_light(&off);
    if (ret == ESP_OK)
        b->lights_owned = false;
    return ret;
}
static esp_err_t light(hc_body_t *b, unsigned argument) {
    if (argument >= 14)
        return ESP_ERR_INVALID_ARG;
    unsigned item = argument % 7;
    mcu_led_request_t r = {.mode = MCU_LED_MODE_STATIC,
                           .zone = argument < 7 ? MCU_LED_ZONE_SIDE : MCU_LED_ZONE_BOTTOM,
                           .brightness = item == 3   ? 24
                                         : item == 4 ? 128
                                                     : 64};
    if (item == 0)
        r.primary_red = 255;
    else if (item == 2)
        r.primary_blue = 255;
    else
        r.primary_green = 255;
    if (item == 5) {
        r.mode = MCU_LED_MODE_EFFECT;
        r.effect_id = MCU_LED_EFFECT_BREATHING;
        r.period_ms = 1200;
    }
    if (item == 6)
        r.mode = MCU_LED_MODE_OFF;
    /* A failed UART write may have submitted a prefix, so still try cleanup. */
    b->lights_owned = true;
    const mcu_led_request_t off = {.mode = MCU_LED_MODE_OFF, .zone = MCU_LED_ZONE_ALL};
    esp_err_t ret = watcher_hw_body_light(&off);
    if (ret == ESP_OK)
        ret = watcher_hw_body_light(&r);
    return ret;
}
esp_err_t hc_body_start(hc_body_t *b, hc_operation_t operation, unsigned argument, uint32_t attempt, int64_t now) {
    if (!b || b->active)
        return ESP_ERR_INVALID_STATE;
    if (!ready())
        return ESP_ERR_INVALID_STATE;
    if (!(operation == HC_OP_TOUCH || operation == HC_OP_LIGHT || operation == HC_OP_MOVE ||
          operation == HC_OP_STOP_TEST))
        return ESP_ERR_INVALID_ARG;
    if ((operation == HC_OP_MOVE || operation == HC_OP_STOP_TEST) && argument > 1)
        return ESP_ERR_INVALID_ARG;
    bool lights_owned = b->lights_owned;
    hc_body_init(b);
    b->lights_owned = lights_owned;
    b->operation = operation;
    b->argument = argument;
    b->attempt = attempt;
    b->active = true;
    b->started_us = now;
    b->deadline_us = now + (operation == HC_OP_TOUCH ? 30000000 : 500000);
    if (operation == HC_OP_TOUCH)
        return ESP_OK;
    if (operation == HC_OP_LIGHT) {
        esp_err_t ret = light(b, argument);
        finish(b, ret);
        return ret;
    }
    /* User-confirmed open-loop reference. This is a command coordinate, not
     * measured position; the UI discloses the reference move before starting. */
    b->x = 90;
    b->y = 120;
    b->preparing = true;
    esp_err_t ret = watcher_hw_body_move(b->x, b->y, 1000, &b->sequence);
    if (ret != ESP_OK)
        finish(b, ret);
    else {
        b->moving = true;
        b->deadline_us = now + 3000000;
    }
    return ret;
}
void hc_body_cancel(hc_body_t *b) {
    if (!b || !b->active)
        return;
    b->cancelled = true;
    b->preparing = false;
    esp_err_t ret = b->moving ? stop(b) : ESP_OK;
    if (!b->moving || ret != ESP_OK)
        finish(b, ret);
}
esp_err_t hc_body_stop_test(hc_body_t *b) {
    if (!b || !b->active || !b->moving || b->stop_submitted || b->operation != HC_OP_STOP_TEST)
        return ESP_ERR_INVALID_STATE;
    esp_err_t ret = stop(b);
    if (ret != ESP_OK)
        finish(b, ret);
    return ret;
}
void hc_body_tick(hc_body_t *b, int64_t now) {
    if (!b || !b->active)
        return;
    if (now >= b->deadline_us) {
        if (b->moving)
            (void)stop(b);
        finish(b, ESP_ERR_TIMEOUT);
    }
}
#if 0 /* Retired: current servos have no physical position feedback. */
static void position(hc_body_t *b, const watcher_hw_event_t *e, int64_t now) {
    if (!b->waiting_position || e->size != sizeof(mcu_motion_servo_feedback_t) || e->received_us < b->started_us ||
        e->received_us > now || now - e->received_us >= 500000 || now >= b->deadline_us) return;
    mcu_motion_servo_feedback_t p;
    memcpy(&p, e->payload, sizeof(p));
    if (p.axis_mask != 3 || p.x_angle_x10 < 0 || p.x_angle_x10 > 1800 || p.y_angle_x10 < 1000 || p.y_angle_x10 > 1400) {
        finish(b, ESP_ERR_INVALID_RESPONSE); return;
    }
    b->waiting_position = false;
    b->x = (p.x_angle_x10 + 5) / 10;
    b->y = (p.y_angle_x10 + 5) / 10;
    if (b->argument == 1) b->y += b->y <= 137 ? 3 : -3;
    else b->x += b->x <= 177 ? 3 : -3;
    uint32_t duration = b->operation == HC_OP_STOP_TEST ? 3000 : 1000;
    esp_err_t ret = watcher_hw_body_move(b->x, b->y, duration, &b->sequence);
    if (ret != ESP_OK) { finish(b, ret); return; }
    b->moving = true;
    b->deadline_us = now + (int64_t)(duration + 2000) * 1000;
}
#endif
static void touch(hc_body_t *b, const watcher_hw_event_t *e) {
    if (b->operation != HC_OP_TOUCH || e->size != sizeof(mcu_touch_state_t))
        return;
    mcu_touch_state_t t;
    memcpy(&t, e->payload, sizeof(t));
    b->last_touch_id = t.touch_id;
    b->last_touch_code = (uint8_t)t.event_code;
    if (b->touch_stage == 0 && t.event_code == MCU_TOUCH_EVENT_PRESS) {
        b->touch_id = t.touch_id;
        b->touch_stage = 1;
    } else if (t.touch_id != b->touch_id)
        b->touch_stage = 0;
    else if (b->touch_stage == 1 && t.event_code == MCU_TOUCH_EVENT_RELEASE)
        b->touch_stage = 2;
    else if (b->touch_stage == 2 && t.event_code == MCU_TOUCH_EVENT_PRESS)
        b->touch_stage = 3;
    else if (b->touch_stage == 3 && t.event_code == MCU_TOUCH_EVENT_LONG_PRESS)
        b->touch_stage = 4;
    else if (b->touch_stage == 4 && t.event_code == MCU_TOUCH_EVENT_RELEASE) {
        b->touch_stage = 5;
        finish(b, ESP_OK);
    } else
        b->touch_stage = 0;
}
void hc_body_event(hc_body_t *b, const watcher_hw_event_t *e, int64_t now) {
    if (!b || !e || !b->active)
        return;
    if (e->type == WATCHER_HW_EVENT_OVERFLOW || e->type == WATCHER_HW_EVENT_LINK) {
        hc_body_cancel(b);
        if (b->error == ESP_OK)
            b->error = e->type == WATCHER_HW_EVENT_OVERFLOW ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_STATE;
        return;
    }
    if (e->received_us < b->started_us)
        return;
    /* Legacy POSITION frames must never compute a target or start movement. */
    if (e->type == WATCHER_HW_EVENT_TOUCH)
        touch(b, e);
    else if (e->type == WATCHER_HW_EVENT_MOTION && b->moving && e->size == sizeof(mcu_motion_lifecycle_event_t)) {
        mcu_motion_lifecycle_event_t m;
        memcpy(&m, e->payload, sizeof(m));
        if (m.ref_seq != b->sequence && !(m.type == MCU_MOTION_LIFECYCLE_FAULT && m.ref_seq == 0))
            return;
        if (m.type == MCU_MOTION_LIFECYCLE_REJECTED || m.type == MCU_MOTION_LIFECYCLE_FAULT)
            finish(b, ESP_FAIL);
        else if (m.type == MCU_MOTION_LIFECYCLE_DONE) {
            if (b->preparing && !b->cancelled && !b->stop_submitted && m.result == MCU_MOTION_RESULT_SUCCESS) {
                b->preparing = false;
                b->x = b->argument == 0 ? 93 : 90;
                b->y = b->argument == 1 ? 123 : 120;
                uint32_t duration = b->operation == HC_OP_STOP_TEST ? 3000 : 1000;
                esp_err_t ret = watcher_hw_body_move(b->x, b->y, duration, &b->sequence);
                if (ret != ESP_OK)
                    finish(b, ret);
                else
                    b->deadline_us = now + (int64_t)(duration + 2000) * 1000;
                return;
            }
            bool success = b->stop_submitted ? m.result == MCU_MOTION_RESULT_STOPPED
                                             : m.result == MCU_MOTION_RESULT_SUCCESS && b->operation != HC_OP_STOP_TEST;
            finish(b, success ? ESP_OK : ESP_FAIL);
        }
    }
}
