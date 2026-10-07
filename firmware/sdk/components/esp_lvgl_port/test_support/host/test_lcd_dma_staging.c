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
} lvgl_port_ctx;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_TIMEOUT 3
static int calls, fail_at;
static lv_color_t captured[64];
static size_t captured_count;
static int last_y;
static esp_err_t lvgl_port_panel_draw_bitmap(void *panel, int x1, int y1, int x2, int y2, const void *data,
                                             uint32_t timeout) {
    (void)panel;
    (void)timeout;
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
#include "lcd_flush_staged.inc"
#define CHECK(c)                                                                                                       \
    do {                                                                                                               \
        if (!(c))                                                                                                      \
            return __LINE__;                                                                                           \
    } while (0)
int main(void) {
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
    puts("LCD staging: exact pixels/rectangles, last partial band, timeout ownership and capacity checks passed");
    return 0;
}
