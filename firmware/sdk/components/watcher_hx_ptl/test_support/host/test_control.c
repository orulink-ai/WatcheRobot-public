#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "watcher_hx_control.h"
#include "watcher_hx_ptl.h"
#include <stdio.h>
#include <string.h>
static int failures, held, bad_boxes;
static unsigned generation, mode, model;
static int64_t clock_us;
static char reply[1024];
static size_t offset;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                                       \
        }                                                                                                              \
    } while (0)
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) {
    return storage;
}
int xSemaphoreTake(SemaphoreHandle_t lock, unsigned timeout) {
    (void)timeout;
    CHECK(!lock->locked);
    lock->locked = 1;
    ++held;
    return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t lock) {
    CHECK(lock->locked);
    lock->locked = 0;
    --held;
    return pdTRUE;
}
int64_t esp_timer_get_time(void) {
    return clock_us;
}
void vTaskDelay(unsigned ticks) {
    clock_us += (int64_t)ticks * 1000;
}
int uart_driver_install(int port, int rx, int tx, int queue, void *handle, int flags) {
    (void)port;
    (void)rx;
    (void)tx;
    (void)queue;
    (void)handle;
    (void)flags;
    CHECK(held == 1);
    return ESP_OK;
}
int uart_driver_delete(int port) {
    (void)port;
    CHECK(held == 1);
    return ESP_OK;
}
int uart_param_config(int port, const uart_config_t *config) {
    (void)port;
    (void)config;
    return ESP_OK;
}
int uart_set_pin(int port, int tx, int rx, int rts, int cts) {
    (void)port;
    (void)tx;
    (void)rx;
    (void)rts;
    (void)cts;
    return ESP_OK;
}
int uart_write_bytes(int port, const void *data, size_t size) {
    (void)port;
    CHECK(held == 1);
    unsigned id = 0, expected = 0;
    const char *request = data;
    if (sscanf(request, "WV1 MODE %u %u %u %u", &id, &expected, &mode, &model) == 4) {
        CHECK(expected == generation);
        ++generation;
    } else if (sscanf(request, "WV1 CATALOG %u", &id) == 1) {
        snprintf(reply, sizeof(reply), "WV1 CATALOG %u 1 2 3 4\r\n", id);
        offset = 0;
        return (int)size;
    } else if (sscanf(request, "WV1 RESULT %u", &id) == 1) {
        snprintf(reply, sizeof(reply), "WV1 BOXES %u %u 2 18 320 240 %u\r\n", id, generation, bad_boxes ? 9 : 0);
        offset = 0;
        return (int)size;
    } else
        CHECK(sscanf(request, "WV1 STATUS %u", &id) == 1);
    /* A stale nonce must not satisfy a new request. */
    snprintf(reply, sizeof(reply), "WV1 STATE 0 0 999 4 0 0 6\r\nWV1 STATE %u 0 %u %u %u 0 0\r\n", id, generation, mode,
             model);
    offset = 0;
    return (int)size;
}
int uart_read_bytes(int port, void *data, size_t size, unsigned timeout) {
    (void)port;
    (void)size;
    CHECK(held == 1);
    if (!reply[offset]) {
        clock_us += (int64_t)timeout * 1000;
        return 0;
    }
    *(char *)data = reply[offset++];
    ++clock_us;
    return 1;
}
esp_err_t watcher_hx_ptl_read(watcher_hx_ptl_frame_t *frame, uint32_t timeout) {
    (void)frame;
    (void)timeout;
    CHECK(held == 1);
    return ESP_ERR_TIMEOUT;
}
int main(void) {
    CHECK(watcher_hx_control_init() == ESP_OK);
    uint32_t crc[4] = {0};
    CHECK(watcher_hx_control_catalog(crc) == ESP_OK && crc[3] == 4);
    CHECK(watcher_hx_control_mode(2, 4) == ESP_OK);
    CHECK(watcher_hx_control_generation() == 1);
    watcher_hx_state_t state = {0};
    CHECK(watcher_hx_control_status(&state) == ESP_OK && state.mode == 2 && state.model == 4 && state.generation == 1);
    watcher_hx_result_t result = {0};
    CHECK(watcher_hx_control_boxes(&result) == ESP_OK && result.sequence == 18 && result.count == 0);
    bad_boxes = 1;
    CHECK(watcher_hx_control_boxes(&result) == ESP_ERR_INVALID_RESPONSE && result.sequence == 18);
    CHECK(watcher_hx_control_mode(0, 0) == ESP_OK);
    CHECK(watcher_hx_control_status(&state) == ESP_OK && state.mode == 0 && state.model == 0);
    watcher_hx_control_deinit();
    CHECK(held == 0);
    return failures ? 1 : 0;
}

