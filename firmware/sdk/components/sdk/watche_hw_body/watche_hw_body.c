#include "watche_hw_body.h"
#include "mcu_runtime.h"
#include "mcu_link_bootstrap.h"
#include "mcu_link_uart.h"
#include "motion_arbiter.h"
#include "mcu_power_service.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
/* Link stale bound assumes the official STM32 periodic sensor stream.
 * Silence invalidates readiness, then bootstrap retries HELLO; no motion replay. */
#define LINK_STALE_US 5000000LL
static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;
static portMUX_TYPE creation_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t worker;
static watche_hw_status_t status;
static watche_hw_event_queue_t queue;
static int64_t last_rx;
static bool stopping;
static SemaphoreHandle_t stopped;
static StaticSemaphore_t stopped_storage;
typedef struct { watche_hw_body_event_cb_t callback; void *context; } subscriber_t;
static subscriber_t subscribers[4];
static bool dispatching;
_Static_assert(sizeof(mcu_motion_lifecycle_event_t) <= WATCHE_HW_EVENT_PAYLOAD_SIZE, "motion event too large");
_Static_assert(sizeof(mcu_touch_state_t) <= WATCHE_HW_EVENT_PAYLOAD_SIZE, "touch event too large");
static void lock(void) {
    portENTER_CRITICAL(&creation_lock);
    if (mutex == NULL) mutex = xSemaphoreCreateMutexStatic(&mutex_storage);
    portEXIT_CRITICAL(&creation_lock);
    xSemaphoreTake(mutex, portMAX_DELAY);
}
static void unlock(void) { xSemaphoreGive(mutex); }
static void publish(watche_hw_event_type_t type, uint32_t seq, const void *payload, size_t size) {
    watche_hw_event_t event = { .type = type, .sequence = seq,
        .received_us = esp_timer_get_time(), .size = size };
    if (size <= sizeof(event.payload)) {
        memcpy(event.payload, payload, size);
        (void)watche_hw_event_push(&queue, &event);
    }
}
static void motion_event(const mcu_motion_lifecycle_event_t *event, void *ctx) {
    (void)ctx;
    /* Invoked synchronously by dispatch while our runtime mutex is held. */
    publish(WATCHE_HW_EVENT_MOTION, event->ref_seq, event, sizeof(*event));
}
static void position_event(const mcu_motion_servo_feedback_t *event, void *ctx) {
    (void)ctx;
    publish(WATCHE_HW_EVENT_POSITION, 0, event, sizeof(*event));
}
static void set_status(watche_hw_state_t state, esp_err_t error) {
    if (status.state == state && status.last_error == error) return;
    status = (watche_hw_status_t){state, error};
    publish(WATCHE_HW_EVENT_LINK, 0, &status, sizeof(status));
}
static void runtime_task(void *ctx) {
    (void)ctx;
    for (;;) {
        lock();
        if (stopping) { unlock(); break; }
        for (unsigned i = 0; i < 16; ++i) {
            mcu_link_event_t event = {0};
            esp_err_t ret = mcu_link_bootstrap_poll(&event);
            if (ret != ESP_OK) {
                if (ret != ESP_ERR_NOT_FOUND) set_status(WATCHE_HW_FAULT, ret);
                break;
            }
            if (event.type == MCU_LINK_RX_EVENT_NONE) break;
            last_rx = esp_timer_get_time();
            if (event.type == MCU_LINK_RX_EVENT_HELLO_RSP) {
                ret = mcu_runtime_complete_baseline(&event);
                mcu_link_peer_info_t peer = {0};
                if (ret == ESP_OK) ret = mcu_link_copy_peer_info(mcu_link_bootstrap_get_link(), &peer);
                const uint8_t required = MCU_CAPABILITY_MOTION | MCU_CAPABILITY_LED | MCU_CAPABILITY_TOUCH;
                if (ret == ESP_OK && (!peer.version_valid || (peer.capability_bitmap & required) != required))
                    ret = ESP_ERR_NOT_SUPPORTED;
                set_status(ret == ESP_OK ? WATCHE_HW_READY : WATCHE_HW_FAULT, ret);
            }
            bool overwritten;
            ret = mcu_runtime_dispatch(&event, &overwritten);
            if (event.type == MCU_LINK_RX_EVENT_TOUCH_EVENT && ret == ESP_OK) {
                mcu_touch_state_t touch;
                if (mcu_sensor_service_get_latest_touch(&touch) == ESP_OK)
                    publish(WATCHE_HW_EVENT_TOUCH, event.frame.header.seq, &touch, sizeof(touch));
            }
        }
        if (status.state == WATCHE_HW_READY && esp_timer_get_time() - last_rx > LINK_STALE_US) {
            mcu_link_t *link = mcu_link_bootstrap_get_link();
            if (link != NULL) (void)mcu_link_mark_degraded(link);
            set_status(WATCHE_HW_FAULT, ESP_ERR_TIMEOUT);
        } else if (status.state == WATCHE_HW_STARTING && mcu_link_bootstrap_handshake_timed_out(5000)) {
            set_status(WATCHE_HW_FAULT, ESP_ERR_TIMEOUT);
        }
        unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    xSemaphoreGive(stopped);
    for (;;) vTaskSuspend(NULL);
}
esp_err_t watche_hw_body_init(void) {
    lock();
    if (worker != NULL) { esp_err_t ret = stopping ? ESP_ERR_INVALID_STATE : ESP_OK; unlock(); return ret; }
    if (mcu_link_bootstrap_get_link() != NULL || motion_arbiter_current_owner() != MOTION_ARBITER_OWNER_NONE) {
        unlock(); return ESP_ERR_INVALID_STATE;
    }
    watche_hw_event_queue_init(&queue);
    memset(subscribers, 0, sizeof(subscribers));
    stopping = false;
    esp_err_t ret = mcu_link_bootstrap_init();
    if (ret == ESP_OK) ret = mcu_motion_service_init();
    if (ret == ESP_OK) ret = mcu_led_service_init();
    if (ret == ESP_OK) ret = mcu_sensor_service_init();
    if (ret == ESP_OK) ret = mcu_power_service_init();
    if (ret == ESP_OK) ret = mcu_motion_set_lifecycle_callback(motion_event, NULL);
    if (ret == ESP_OK) ret = mcu_motion_set_servo_feedback_callback(position_event, NULL);
    if (ret == ESP_OK) ret = mcu_link_bootstrap_start();
    if (ret == ESP_OK) {
        if (stopped == NULL) stopped = xSemaphoreCreateBinaryStatic(&stopped_storage);
        (void)xSemaphoreTake(stopped, 0);
        last_rx = esp_timer_get_time();
        set_status(WATCHE_HW_STARTING, ESP_OK);
        if (xTaskCreate(runtime_task, "watche_body", 4096, NULL, 5, &worker) != pdPASS) ret = ESP_ERR_NO_MEM;
    }
    if (ret != ESP_OK) {
        (void)mcu_motion_set_lifecycle_callback(NULL, NULL);
        (void)mcu_motion_set_servo_feedback_callback(NULL, NULL);
        mcu_link_bootstrap_stop();
        set_status(WATCHE_HW_FAULT, ret);
    }
    unlock(); return ret;
}
esp_err_t watche_hw_body_close(void) {
    lock();
    if (worker == NULL) { status = (watche_hw_status_t){WATCHE_HW_CLOSED, ESP_OK}; unlock(); return ESP_OK; }
    /* Retry a timed-out shutdown without restarting an already suspended task. */
    stopping = true;
    esp_err_t ret = mcu_link_bootstrap_is_ready() ? mcu_motion_stop(MCU_MOTION_SOURCE_UNKNOWN) : ESP_ERR_INVALID_STATE;
    if (ret == ESP_OK) ret = mcu_link_uart_wait_tx_done(250);
    unlock();
    if (xSemaphoreTake(stopped, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    lock();
    vTaskDelete(worker); worker = NULL;
    (void)mcu_motion_set_lifecycle_callback(NULL, NULL);
    (void)mcu_motion_set_servo_feedback_callback(NULL, NULL);
    mcu_link_bootstrap_stop();
    set_status(WATCHE_HW_CLOSED, ret);
    memset(subscribers, 0, sizeof(subscribers));
    stopping = false;
    unlock(); return ret;
}
esp_err_t watche_hw_body_status(watche_hw_status_t *out) {
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    lock(); *out = status; unlock(); return ESP_OK;
}
static bool ready(void) { return worker != NULL && !stopping && status.state == WATCHE_HW_READY && mcu_link_bootstrap_is_ready(); }
esp_err_t watche_hw_body_move(int x, int y, uint32_t duration, uint32_t *seq) {
    if (x < 0 || x > 180 || y < 100 || y > 140 || duration == 0 || duration > UINT16_MAX) return ESP_ERR_INVALID_ARG;
    mcu_motion_request_t request = { .axis_mask = MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y,
        .x_deg_x10 = (int16_t)(x * 10), .y_deg_x10 = (int16_t)(y * 10), .duration_ms = (uint16_t)duration,
        .motion_profile = MCU_MOTION_PROFILE_EASE_IN_OUT, .source = MCU_MOTION_SOURCE_UNKNOWN };
    lock(); esp_err_t ret = ready() ? mcu_motion_submit_with_seq(&request, seq) : ESP_ERR_INVALID_STATE; unlock(); return ret;
}
esp_err_t watche_hw_body_stop(void) {
    lock(); esp_err_t ret = ready() ? mcu_motion_stop(MCU_MOTION_SOURCE_UNKNOWN) : ESP_ERR_INVALID_STATE; unlock(); return ret;
}
esp_err_t watche_hw_body_request_position(void) {
    lock();
    esp_err_t ret = ready() ? mcu_motion_request_feedback() : ESP_ERR_INVALID_STATE;
    unlock();
    return ret;
}
esp_err_t watche_hw_body_light(const mcu_led_request_t *request) {
    if (request == NULL) return ESP_ERR_INVALID_ARG;
    lock(); esp_err_t ret = ready() ? mcu_led_submit(request) : ESP_ERR_INVALID_STATE; unlock(); return ret;
}
esp_err_t watche_hw_body_next_event(watche_hw_event_t *event) {
    lock(); esp_err_t ret = watche_hw_event_pop(&queue, event); unlock(); return ret;
}
esp_err_t watche_hw_body_dropped_events(uint32_t *out) {
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    lock(); *out = queue.dropped; unlock(); return ESP_OK;
}
esp_err_t watche_hw_body_subscribe(watche_hw_body_event_cb_t callback, void *context) {
    if (callback == NULL) return ESP_ERR_INVALID_ARG;
    lock();
    if (worker == NULL || stopping) { unlock(); return ESP_ERR_INVALID_STATE; }
    int free_slot = -1;
    for (unsigned i = 0; i < 4; ++i) {
        if (subscribers[i].callback == callback && subscribers[i].context == context) {
            unlock(); return ESP_ERR_INVALID_STATE;
        }
        if (subscribers[i].callback == NULL && free_slot < 0) free_slot = (int)i;
    }
    if (free_slot < 0) { unlock(); return ESP_ERR_NO_MEM; }
    subscribers[free_slot] = (subscriber_t){callback, context};
    unlock(); return ESP_OK;
}
esp_err_t watche_hw_body_unsubscribe(watche_hw_body_event_cb_t callback, void *context) {
    if (callback == NULL) return ESP_ERR_INVALID_ARG;
    lock();
    for (unsigned i = 0; i < 4; ++i) {
        if (subscribers[i].callback == callback && subscribers[i].context == context) {
            subscribers[i] = (subscriber_t){0}; unlock(); return ESP_OK;
        }
    }
    unlock(); return ESP_ERR_NOT_FOUND;
}
esp_err_t watche_hw_body_dispatch_events(uint32_t max_events, uint32_t *dispatched) {
    if (max_events == 0 || dispatched == NULL) return ESP_ERR_INVALID_ARG;
    *dispatched = 0;
    lock();
    if (dispatching || worker == NULL || stopping) { unlock(); return ESP_ERR_INVALID_STATE; }
    dispatching = true;
    unlock();
    for (uint32_t i = 0; i < max_events; ++i) {
        watche_hw_event_t event; subscriber_t snapshot[4];
        lock();
        esp_err_t ret = watche_hw_event_pop(&queue, &event);
        memcpy(snapshot, subscribers, sizeof(snapshot));
        unlock();
        if (ret != ESP_OK) break;
        for (unsigned n = 0; n < 4; ++n)
            if (snapshot[n].callback != NULL) snapshot[n].callback(&event, snapshot[n].context);
        ++*dispatched;
    }
    lock(); dispatching = false; unlock();
    return ESP_OK;
}

