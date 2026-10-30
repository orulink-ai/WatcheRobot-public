#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int esp_err_t;
typedef uint16_t lv_color_t;
typedef struct {
    int x1, y1, x2, y2;
} lv_area_t;
typedef struct {
    void *panel_handle;
    lv_color_t *trans_buffer;
    size_t trans_buffer_size;
} lvgl_port_display_ctx_t;
static struct {
    bool panel_direct_draw_pending;
    void *panel_mux;
} lvgl_port_ctx;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_TIMEOUT 3
static int calls, fail_at;
static bool mutex_held;
static unsigned take_attempts, acquisitions, releases;
static lv_color_t captured[64];
static size_t captured_count;
static int last_y;
static esp_err_t lvgl_port_panel_draw_bitmap_locked(void *panel, int x1, int y1, int x2, int y2, const void *data,
                                             uint32_t timeout) {
    assert(panel != NULL && mutex_held);
    assert(timeout == 50);
    ++calls;
    if (calls == fail_at) {
        lvgl_port_ctx.panel_direct_draw_pending = true;
        return ESP_ERR_TIMEOUT;
    }
    if (x1 != 3 || x2 != 8 || y1 != last_y || y2 <= y1 || y2 - y1 > 2)
        return ESP_ERR_INVALID_ARG;
    size_t pixels = (size_t)(x2 - x1) * (size_t)(y2 - y1);
    memcpy(captured + captured_count, data, pixels * sizeof(lv_color_t));
    captured_count += pixels;
    last_y = y2;
    return ESP_OK;
}
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
static int xSemaphoreTake(void *mutex, unsigned ticks) {
    assert(mutex == &lvgl_port_ctx && ticks == pdMS_TO_TICKS(50));
    ++take_attempts;
    if (mutex_held)
        return 0;
    mutex_held = true;
    ++acquisitions;
    return pdTRUE;
}
static void xSemaphoreGive(void *mutex) {
    assert(mutex == &lvgl_port_ctx && mutex_held);
    mutex_held = false;
    ++releases;
}
#include "lcd_draw_staged_locked.inc"
#include "lcd_flush_staged.inc"
#define CHECK(c)                                                                                                       \
    do {                                                                                                               \
        if (!(c))                                                                                                      \
            return __LINE__;                                                                                           \
    } while (0)
int main(void) {
    lvgl_port_ctx.panel_mux = &lvgl_port_ctx;
    lv_color_t source[25], staging[10];
    for (unsigned i = 0; i < 25; ++i)
        source[i] = (lv_color_t)(0x1200 + i);
    lvgl_port_display_ctx_t ctx = {.panel_handle = &ctx, .trans_buffer = staging, .trans_buffer_size = 10};
    lv_area_t area = {3, 7, 7, 11};
    last_y = 7;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_OK);
    CHECK(calls == 3 && last_y == 12 && captured_count == 25);
    CHECK(memcmp(source, captured, sizeof(source)) == 0);
    calls = 0;
    fail_at = 2;
    last_y = 7;
    captured_count = 0;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_TIMEOUT && calls == 2);
    lv_color_t retained[10];
    memcpy(retained, staging, sizeof(staging));
    memset(source, 0, sizeof(source));
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_INVALID_STATE);
    CHECK(calls == 2 && memcmp(retained, staging, sizeof(staging)) == 0);
    lvgl_port_ctx.panel_direct_draw_pending = false;
    calls = 0;
    fail_at = 0;
    last_y = 7;
    captured_count = 0;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_OK && calls == 3);
    CHECK(memcmp(source, captured, sizeof(source)) == 0);
    calls = 2;
    ctx.trans_buffer_size = 4;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_INVALID_ARG && calls == 2);
    area.x2 = area.x1 - 1;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_INVALID_ARG);
    CHECK(!mutex_held && acquisitions == releases);
    /* Invalid input must not borrow the staging buffer or leak the mutex. */
    area = (lv_area_t){3, 7, 7, 11};
    ctx.trans_buffer_size = 10;
    memcpy(retained, staging, sizeof(staging));
    unsigned attempts_before = take_attempts, releases_before = releases;
    lvgl_port_ctx.panel_mux = NULL;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_INVALID_ARG);
    lvgl_port_ctx.panel_mux = &lvgl_port_ctx;
    CHECK(lvgl_port_flush_staged(&ctx, NULL, source) == ESP_ERR_INVALID_ARG);
    CHECK(take_attempts == attempts_before && releases == releases_before);
    /* Contention must neither draw, mutate DMA storage nor give another owner's mutex. */
    int calls_before = calls;
    mutex_held = true;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_TIMEOUT);
    CHECK(mutex_held && take_attempts == attempts_before + 1 && releases == releases_before);
    CHECK(calls == calls_before && memcmp(retained, staging, sizeof(staging)) == 0);
    mutex_held = false;
    CHECK(lvgl_port_flush_staged(NULL, &area, source) == ESP_ERR_INVALID_ARG);
    CHECK(!mutex_held && acquisitions == releases);
    CHECK(lvgl_port_flush_staged(&ctx, &area, NULL) == ESP_ERR_INVALID_ARG);
    CHECK(!mutex_held && acquisitions == releases);
    ctx.trans_buffer = NULL;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_INVALID_ARG);
    ctx.trans_buffer = staging;
    area.y2 = area.y1 - 1;
    CHECK(lvgl_port_flush_staged(&ctx, &area, source) == ESP_ERR_INVALID_ARG);
    CHECK(!mutex_held && acquisitions == releases && calls == calls_before);
    CHECK(memcmp(retained, staging, sizeof(staging)) == 0);
    puts("LCD staging: exact pixels/rectangles, last partial band, timeout ownership and capacity checks passed");
    return 0;
}
