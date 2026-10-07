/* Cooperative fake scheduler: one runtime iteration per tick, no real UART.
 * This tests orchestration; protocol encoding is covered by shared suites. */
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mcu_link_bootstrap.h"
#include "mcu_runtime.h"
#include "motion_arbiter.h"
#include "watche_hw_body.h"
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
static jmp_buf boundary;
static void (*worker_fn)(void *);
static bool link_active, link_ready, incompatible, task_fail, shutdown_timeout;
static int stage, fail_stage, stops, moves, deletes;
static int64_t now;
static mcu_link_t link;
static mcu_link_event_t incoming[128];
static size_t read_index, write_index;
static mcu_touch_state_t latest_touch;
static mcu_motion_lifecycle_cb_t lifecycle;
static mcu_motion_servo_feedback_cb_t feedback;
static mcu_motion_request_t submitted;
static bool uart_busy;
static esp_err_t poll_error;
static unsigned probes;
bool mcu_link_uart_try_lock(void) {
    return !uart_busy;
}
void mcu_link_uart_unlock(void) {}
static esp_err_t step(void) {
    return ++stage == fail_stage ? ESP_FAIL : ESP_OK;
}
static void tick(void) {
    assert(worker_fn);
    if (setjmp(boundary) == 0)
        worker_fn(NULL);
}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *s) {
    s->binary = 0;
    s->count = 1;
    return s;
}
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *s) {
    s->binary = 1;
    s->count = 0;
    return s;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t timeout) {
    if (!s->binary)
        return pdTRUE;
    if (!s->count && timeout && !shutdown_timeout)
        tick();
    if (!s->count)
        return 0;
    s->count = 0;
    return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
    s->count = 1;
    return pdTRUE;
}
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack, void *ctx, unsigned priority,
                       TaskHandle_t *task) {
    (void)name;
    (void)stack;
    (void)ctx;
    (void)priority;
    if (task_fail)
        return 0;
    worker_fn = fn;
    *task = &link;
    return pdPASS;
}
void vTaskDelete(TaskHandle_t task) {
    assert(task);
    worker_fn = NULL;
    deletes++;
}
void vTaskSuspend(TaskHandle_t task) {
    (void)task;
    longjmp(boundary, 1);
}
void vTaskDelay(TickType_t ticks) {
    (void)ticks;
    longjmp(boundary, 1);
}
int64_t esp_timer_get_time(void) {
    return now;
}
mcu_link_t *mcu_link_bootstrap_get_link(void) {
    return link_active ? &link : NULL;
}
bool mcu_link_bootstrap_is_ready(void) {
    return link_ready;
}
bool mcu_link_bootstrap_handshake_timed_out(uint32_t timeout) {
    return now > (int64_t)timeout * 1000;
}
esp_err_t mcu_link_bootstrap_init(void) {
    esp_err_t ret = step();
    if (!ret)
        link_active = true;
    return ret;
}
esp_err_t mcu_link_bootstrap_start(void) {
    return step();
}
void mcu_link_bootstrap_stop(void) {
    link_active = link_ready = false;
    stops++;
}
esp_err_t mcu_link_bootstrap_poll(mcu_link_event_t *out) {
    if (uart_busy)
        return ESP_ERR_INVALID_STATE;
    if (poll_error != ESP_OK) {
        esp_err_t error = poll_error;
        poll_error = ESP_OK;
        return error;
    }
    if (read_index == write_index)
        return ESP_ERR_NOT_FOUND;
    *out = incoming[read_index++];
    return ESP_OK;
}
esp_err_t mcu_link_copy_peer_info(const mcu_link_t *value, mcu_link_peer_info_t *out) {
    assert(value == &link);
    memset(out, 0, sizeof(*out));
    out->version_valid = true;
    out->capability_bitmap = incompatible ? 0 : 7;
    return ESP_OK;
}
esp_err_t mcu_link_mark_degraded(mcu_link_t *value) {
    assert(value == &link);
    link_ready = false;
    return ESP_OK;
}
esp_err_t mcu_link_send_frame(mcu_link_t *value, uint8_t cls, uint8_t id, uint8_t flags, const uint8_t *payload,
                              uint16_t size, uint32_t *seq, size_t *wire_size) {
    assert(value == &link && cls == MCU_FRAME_CLASS_SYS && id == MCU_SYS_MSG_HELLO_REQ);
    assert(flags == MCU_FRAME_FLAG_ACK_REQ && payload == NULL && size == 0);
    (void)seq;
    (void)wire_size;
    ++probes;
    return ESP_OK;
}
esp_err_t mcu_runtime_complete_baseline(const mcu_link_event_t *event) {
    (void)event;
    link_ready = true;
    return ESP_OK;
}
esp_err_t mcu_motion_service_init(void) {
    return step();
}
esp_err_t mcu_led_service_init(void) {
    return step();
}
esp_err_t mcu_sensor_service_init(void) {
    return step();
}
esp_err_t mcu_power_service_init(void) {
    return step();
}
esp_err_t mcu_motion_set_lifecycle_callback(mcu_motion_lifecycle_cb_t cb, void *ctx) {
    (void)ctx;
    lifecycle = cb;
    return ESP_OK;
}
esp_err_t mcu_motion_set_servo_feedback_callback(mcu_motion_servo_feedback_cb_t cb, void *ctx) {
    (void)ctx;
    feedback = cb;
    return ESP_OK;
}
motion_arbiter_owner_t motion_arbiter_current_owner(void) {
    return MOTION_ARBITER_OWNER_NONE;
}
esp_err_t mcu_motion_submit_with_seq(const mcu_motion_request_t *request, uint32_t *seq) {
    submitted = *request;
    moves++;
    if (seq)
        *seq = 42;
    return ESP_OK;
}
esp_err_t mcu_motion_stop(mcu_motion_source_t source) {
    (void)source;
    return ESP_OK;
}
esp_err_t mcu_motion_request_feedback(void) {
    return ESP_OK;
}
esp_err_t mcu_link_uart_wait_tx_done(uint32_t timeout) {
    assert(timeout == 250);
    return ESP_OK;
}
esp_err_t mcu_led_submit(const mcu_led_request_t *request) {
    (void)request;
    return ESP_OK;
}
esp_err_t mcu_sensor_service_get_latest_touch(mcu_touch_state_t *out) {
    *out = latest_touch;
    return ESP_OK;
}
esp_err_t mcu_runtime_dispatch(const mcu_link_event_t *event, bool *overwritten) {
    *overwritten = false;
    if (event->type == MCU_LINK_RX_EVENT_TOUCH_EVENT) {
        latest_touch = (mcu_touch_state_t){1, (mcu_touch_event_code_t)event->frame.payload[0], true, 0};
    } else if (event->type == MCU_LINK_RX_EVENT_MOTION_DONE && lifecycle) {
        mcu_motion_lifecycle_event_t result = {0};
        result.type = (mcu_motion_lifecycle_event_type_t)event->frame.payload[0];
        result.ref_seq = 42;
        lifecycle(&result, NULL);
    }
    return ESP_OK;
}
static void enqueue(mcu_link_rx_event_type_t type, uint8_t code) {
    assert(write_index < 128);
    incoming[write_index] = (mcu_link_event_t){0};
    incoming[write_index].type = type;
    incoming[write_index].frame.payload[0] = code;
    incoming[write_index].frame.header.seq = (uint32_t)write_index;
    write_index++;
}
static void drain(void) {
    watche_hw_event_t event;
    while (watche_hw_body_next_event(&event) == ESP_OK) {
    }
}
static unsigned received;
static void subscriber(const watche_hw_event_t *event, void *ctx) {
    assert(ctx == &received);
    if (event->type == WATCHE_HW_EVENT_TOUCH) {
        mcu_touch_state_t touch;
        memcpy(&touch, event->payload, sizeof(touch));
        assert((unsigned)touch.event_code == ++received);
        watche_hw_status_t state;
        assert(watche_hw_body_status(&state) == ESP_OK);
        assert(watche_hw_body_close() == ESP_ERR_INVALID_STATE);
        assert(watche_hw_body_init() == ESP_ERR_INVALID_STATE);
    }
}
int main(void) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    watche_hw_status_t state;
    uint32_t seq, dropped;
    watche_hw_event_t event;
    assert(watche_hw_body_move(90, 120, 1000, &seq) == ESP_ERR_INVALID_STATE);
    assert(watche_hw_body_move(-1, 120, 1000, &seq) == ESP_ERR_INVALID_ARG);
    assert(watche_hw_body_move(90, 99, 1000, &seq) == ESP_ERR_INVALID_ARG);
    assert(watche_hw_body_move(90, 120, 65536, &seq) == ESP_ERR_INVALID_ARG);
    assert(watche_hw_body_light(NULL) == ESP_ERR_INVALID_ARG);
    assert(watche_hw_body_status(NULL) == ESP_ERR_INVALID_ARG);
    link_active = true;
    assert(watche_hw_body_init() == ESP_ERR_INVALID_STATE);
    link_active = false;
    for (int i = 1; i <= 6; i++) {
        stage = 0;
        fail_stage = i;
        int before = stops;
        assert(watche_hw_body_init() == ESP_FAIL && stops == before + 1 && !link_active);
        assert(!lifecycle && !feedback);
        assert(watche_hw_body_close() == ESP_OK);
    }
    fail_stage = 0;
    task_fail = true;
    assert(watche_hw_body_init() == ESP_ERR_NO_MEM && !link_active);
    task_fail = false;
    for (int cycle = 0; cycle < 3; cycle++) {
        now = 0;
        stage = 0;
        read_index = write_index = 0;
        assert(watche_hw_body_init() == ESP_OK);
        assert(watche_hw_body_init() == ESP_OK);
        assert(watche_hw_body_move(90, 120, 1000, &seq) == ESP_ERR_INVALID_STATE);
        now = 6000000;
        tick();
        assert(watche_hw_body_status(&state) == ESP_OK && state.last_error == ESP_ERR_TIMEOUT);
        incompatible = true;
        enqueue(MCU_LINK_RX_EVENT_HELLO_RSP, 0);
        tick();
        assert(watche_hw_body_status(&state) == ESP_OK && state.last_error == ESP_ERR_NOT_SUPPORTED);
        assert(watche_hw_body_stop() == ESP_ERR_INVALID_STATE);
        incompatible = false;
        enqueue(MCU_LINK_RX_EVENT_HELLO_RSP, 0);
        tick();
        drain();
        /* UART diagnostic readers can briefly own the gate. This must not
         * latch a body fault while the peer remains ready. */
        uart_busy = true;
        tick();
        uart_busy = false;
        assert(watche_hw_body_status(&state) == ESP_OK && state.state == WATCHE_HW_READY);
        /* Recoverable transport errors must schedule a fresh handshake. */
        poll_error = ESP_FAIL;
        tick();
        assert(watche_hw_body_status(&state) == ESP_OK && state.state == WATCHE_HW_FAULT);
        assert(!link_ready);
        enqueue(MCU_LINK_RX_EVENT_HELLO_RSP, 0);
        tick();
        drain();
        /* Quiet firmware has no periodic sensor stream. Check link health
         * with a control-plane HELLO, never position/sensor queries. */
        unsigned before_probes = probes;
        now += 2100000;
        tick();
        assert(probes == before_probes + 1 && link_ready);
        assert(watche_hw_body_status(&state) == ESP_OK && state.state == WATCHE_HW_READY);
        enqueue(MCU_LINK_RX_EVENT_HELLO_RSP, 0);
        tick();
        drain();
        received = 0;
        uint32_t delivered;
        assert(watche_hw_body_subscribe(NULL, NULL) == ESP_ERR_INVALID_ARG);
        assert(watche_hw_body_subscribe(subscriber, &received) == ESP_OK);
        assert(watche_hw_body_subscribe(subscriber, &received) == ESP_ERR_INVALID_STATE);
        for (int code = 1; code <= 3; code++)
            enqueue(MCU_LINK_RX_EVENT_TOUCH_EVENT, (uint8_t)code);
        tick();
        assert(watche_hw_body_dispatch_events(0, &delivered) == ESP_ERR_INVALID_ARG);
        assert(watche_hw_body_dispatch_events(2, &delivered) == ESP_OK && delivered == 2 && received == 2);
        assert(watche_hw_body_dispatch_events(8, &delivered) == ESP_OK && delivered == 1 && received == 3);
        assert(watche_hw_body_unsubscribe(subscriber, &received) == ESP_OK);
        assert(watche_hw_body_unsubscribe(subscriber, &received) == ESP_ERR_NOT_FOUND);
        assert(watche_hw_body_move(90, 120, 1000, &seq) == ESP_OK && seq == 42);
        assert(submitted.x_deg_x10 == 900 && submitted.y_deg_x10 == 1200 && submitted.duration_ms == 1000);
        for (int code = 1; code <= 3; code++)
            enqueue(MCU_LINK_RX_EVENT_TOUCH_EVENT, (uint8_t)code);
        for (int code = 1; code <= 3; code++)
            enqueue(MCU_LINK_RX_EVENT_MOTION_DONE, (uint8_t)code);
        tick();
        for (int code = 1; code <= 3; code++) {
            mcu_touch_state_t touch;
            assert(watche_hw_body_next_event(&event) == ESP_OK);
            assert(event.type == WATCHE_HW_EVENT_TOUCH);
            memcpy(&touch, event.payload, sizeof(touch));
            assert((int)touch.event_code == code);
        }
        for (int code = 1; code <= 3; code++) {
            mcu_motion_lifecycle_event_t result;
            assert(watche_hw_body_next_event(&event) == ESP_OK);
            assert(event.type == WATCHE_HW_EVENT_MOTION);
            memcpy(&result, event.payload, sizeof(result));
            assert((int)result.type == code && result.ref_seq == 42);
        }
        for (int i = 0; i < 40; i++)
            enqueue(MCU_LINK_RX_EVENT_TOUCH_EVENT, 1);
        tick();
        tick();
        tick();
        assert(watche_hw_body_dropped_events(&dropped) == ESP_OK && dropped == 8);
        drain();
        int before = moves;
        now += 6000000;
        tick();
        assert(watche_hw_body_move(90, 120, 1000, &seq) == ESP_ERR_INVALID_STATE);
        enqueue(MCU_LINK_RX_EVENT_HELLO_RSP, 0);
        tick();
        assert(moves == before); /* Recovery does not replay motion. */
        shutdown_timeout = true;
        assert(watche_hw_body_close() == ESP_ERR_TIMEOUT);
        assert(watche_hw_body_init() == ESP_ERR_INVALID_STATE);
        assert(watche_hw_body_move(90, 120, 1000, &seq) == ESP_ERR_INVALID_STATE);
        shutdown_timeout = false;
        assert(watche_hw_body_close() == ESP_OK);
        assert(!link_active && !lifecycle && !feedback);
        assert(watche_hw_body_close() == ESP_OK);
    }
    assert(deletes == 3);
    return 0;
}
