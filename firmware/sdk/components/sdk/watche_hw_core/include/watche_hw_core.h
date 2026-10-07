#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define WATCHE_HW_EVENT_CAPACITY 32
#define WATCHE_HW_EVENT_PAYLOAD_SIZE 48
typedef enum { WATCHE_HW_CLOSED, WATCHE_HW_STARTING, WATCHE_HW_READY, WATCHE_HW_FAULT } watche_hw_state_t;
typedef struct {
    watche_hw_state_t state;
    esp_err_t last_error;
} watche_hw_status_t;
typedef enum {
    WATCHE_HW_EVENT_LINK,
    WATCHE_HW_EVENT_TOUCH,
    WATCHE_HW_EVENT_MOTION,
    WATCHE_HW_EVENT_POSITION
} watche_hw_event_type_t;
typedef struct {
    watche_hw_event_type_t type;
    uint32_t sequence;
    int64_t received_us; /* esp_timer monotonic timestamp; copied at publication */
    size_t size;
    uint8_t payload[WATCHE_HW_EVENT_PAYLOAD_SIZE];
} watche_hw_event_t;
/* Caller-owned FIFO. SDK modules serialize access; standalone users must do so too.
 * On overflow preserve older events, drop newest and increment dropped. */
typedef struct {
    watche_hw_event_t events[WATCHE_HW_EVENT_CAPACITY];
    size_t head, count;
    uint32_t dropped;
} watche_hw_event_queue_t;
void watche_hw_event_queue_init(watche_hw_event_queue_t *queue);
esp_err_t watche_hw_event_push(watche_hw_event_queue_t *queue, const watche_hw_event_t *event);
esp_err_t watche_hw_event_pop(watche_hw_event_queue_t *queue, watche_hw_event_t *event);
#ifdef __cplusplus
}
#endif
