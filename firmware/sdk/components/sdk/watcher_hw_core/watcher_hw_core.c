#include "watcher_hw_core.h"
#include <string.h>
void watcher_hw_event_queue_init(watcher_hw_event_queue_t *queue) {
    if (queue != NULL)
        memset(queue, 0, sizeof(*queue));
}
esp_err_t watcher_hw_event_push(watcher_hw_event_queue_t *queue, const watcher_hw_event_t *event) {
    if (queue == NULL || event == NULL || event->size > WATCHER_HW_EVENT_PAYLOAD_SIZE)
        return ESP_ERR_INVALID_ARG;
    if (queue->count == WATCHER_HW_EVENT_CAPACITY) {
        queue->dropped++;
        return ESP_ERR_NO_MEM;
    }
    queue->events[(queue->head + queue->count) % WATCHER_HW_EVENT_CAPACITY] = *event;
    queue->count++;
    return ESP_OK;
}
esp_err_t watcher_hw_event_pop(watcher_hw_event_queue_t *queue, watcher_hw_event_t *event) {
    if (queue == NULL || event == NULL)
        return ESP_ERR_INVALID_ARG;
    if (queue->count == 0)
        return ESP_ERR_NOT_FOUND;
    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % WATCHER_HW_EVENT_CAPACITY;
    queue->count--;
    return ESP_OK;
}
