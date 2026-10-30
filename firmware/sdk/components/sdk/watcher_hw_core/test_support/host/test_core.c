#include "watcher_hw_core.h"
#include <assert.h>
#include <string.h>
int main(void) {
    watcher_hw_event_queue_t q;
    watcher_hw_event_t event = {.type = WATCHER_HW_EVENT_TOUCH}, out;
    watcher_hw_event_queue_init(&q);
    assert(watcher_hw_event_pop(&q, &out) == ESP_ERR_NOT_FOUND);
    assert(watcher_hw_event_push(NULL, &event) == ESP_ERR_INVALID_ARG);
    event.size = WATCHER_HW_EVENT_PAYLOAD_SIZE + 1;
    assert(watcher_hw_event_push(&q, &event) == ESP_ERR_INVALID_ARG);
    event.size = 1;
    for (unsigned cycle = 0; cycle < 3; cycle++) {
        for (unsigned i = 0; i < WATCHER_HW_EVENT_CAPACITY; i++) {
            event.sequence = i;
            event.received_us = 1000 + i;
            event.payload[0] = (uint8_t)(i % 3 + 1);
            assert(watcher_hw_event_push(&q, &event) == ESP_OK);
        }
        assert(watcher_hw_event_push(&q, &event) == ESP_ERR_NO_MEM);
        assert(q.dropped == cycle + 1);
        for (unsigned i = 0; i < WATCHER_HW_EVENT_CAPACITY; i++) {
            assert(watcher_hw_event_pop(&q, &out) == ESP_OK);
            assert(out.sequence == i && out.payload[0] == i % 3 + 1);
            assert(out.received_us == 1000 + i);
        }
    }
    watcher_hw_event_queue_init(&q);
    assert(q.count == 0 && q.dropped == 0);
    return 0;
}
