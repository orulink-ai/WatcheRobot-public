#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define WATCHER_HW_EVENT_CAPACITY 32
#define WATCHER_HW_EVENT_PAYLOAD_SIZE 48
typedef enum { WATCHER_HW_CLOSED, WATCHER_HW_STARTING, WATCHER_HW_READY, WATCHER_HW_FAULT } watcher_hw_state_t;
typedef struct {
    watcher_hw_state_t state;
    esp_err_t last_error;
} watcher_hw_status_t;
typedef enum {
    WATCHER_HW_EVENT_LINK,
    WATCHER_HW_EVENT_TOUCH,
    WATCHER_HW_EVENT_MOTION,
    WATCHER_HW_EVENT_POSITION,
    WATCHER_HW_EVENT_OVERFLOW
} watcher_hw_event_type_t;
typedef struct {
    watcher_hw_event_type_t type;
    uint32_t sequence;
    int64_t received_us; /* esp_timer monotonic timestamp; copied at publication */
    size_t size;
    uint8_t payload[WATCHER_HW_EVENT_PAYLOAD_SIZE];
} watcher_hw_event_t;
/* Caller-owned FIFO. SDK modules serialize access; standalone users must do so too.
 * On overflow preserve older events, drop newest and increment dropped. */
typedef struct {
    watcher_hw_event_t events[WATCHER_HW_EVENT_CAPACITY];
    size_t head, count;
    uint32_t dropped;
} watcher_hw_event_queue_t;
void watcher_hw_event_queue_init(watcher_hw_event_queue_t *queue);
esp_err_t watcher_hw_event_push(watcher_hw_event_queue_t *queue, const watcher_hw_event_t *event);
esp_err_t watcher_hw_event_pop(watcher_hw_event_queue_t *queue, watcher_hw_event_t *event);
#ifdef __cplusplus
}
#endif
