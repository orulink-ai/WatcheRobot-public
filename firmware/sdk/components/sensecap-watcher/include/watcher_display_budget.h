#pragma once
#include <stdbool.h>
#include <stddef.h>

/* PSRAM drawing and internal DMA staging have different budgets. A large draw
 * area must not require an equally large contiguous internal allocation. */
static inline size_t watcher_lcd_transfer_budget(size_t configured, bool claw_enabled) {
    return claw_enabled && configured > 4092 ? 4092 : configured;
}
static inline size_t watcher_lvgl_draw_lines(size_t configured, bool claw_enabled) {
    return claw_enabled ? 412 : configured;
}
static inline size_t watcher_lcd_queue_depth(size_t configured, bool claw_enabled) {
    return claw_enabled ? 1 : configured;
}

