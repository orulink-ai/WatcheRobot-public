#include "host_mocks.h"
#include "esp_heap_caps.h"
#include "watcher_hw_display.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#error Host regression assertions must remain enabled in Release builds
#endif
unsigned host_image_releases;
void *heap_caps_malloc(size_t bytes, unsigned caps) {
    (void)caps;
    return malloc(bytes);
}
void heap_caps_free(void *memory) { free(memory); }
bool watcher_hw_display_lock(uint32_t timeout_ms) { (void)timeout_ms; return true; }
void watcher_hw_display_unlock(void) {}
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t ticks) {
    (void)ticks;
    assert(queue && item);
    if (queue->count == 32) return pdFALSE;
    queue->items[queue->count++] = *(const check_input_t *)item;
    return pdTRUE;
}
/* The view receives already decoded RGB565; JPEG/camera is deliberately absent.
 * Zoom is the actual common/demo_state.c function, never a substitute. */
void demo_image_release(demo_image_t *image) {
    if (!image) return;
    if (image->pixels) host_image_releases++;
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}
